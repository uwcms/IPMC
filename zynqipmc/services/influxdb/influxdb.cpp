/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

// TODO: Consider implementing retries if failed to push metrics

#include <core.h>
#include "influxdb.h"
#include <chrono>
#include <services/persistentstorage/persistent_storage.h>
#include <libs/threading.h>
#include <services/console/consolesvc.h>

using namespace std::chrono;

InfluxDB::InfluxDB(LogTree &logtree, const Config& config, const std::string& thread_name) :
logtree(logtree), flush_ticks(0), pushed_metrics(0), dropped_metrics(0) {
	this->collector_mutex = xSemaphoreCreateMutex();
	this->flush_mutex = xSemaphoreCreateMutex();
	this->collector = std::unique_ptr<MetricSet>(new MetricSet());
	configASSERT(this->collector_mutex && this->flush_mutex && this->collector)

	// Load configuration
	uint16_t psver = persistent_storage->getSectionVersion(PersistentStorageAllocations::WISC_INFLUXDB_CONFIG);
	if (psver > 0 && psver != InfluxDB::ConfigVersion) {
		// Record exists but it is old, erase it.
		logtree.log("Out-dated configuration record, replacing with defaults.", LogTree::LOG_WARNING);
		persistent_storage->deleteSection(PersistentStorageAllocations::WISC_INFLUXDB_CONFIG);
		psver = 0;
	}

	this->config = (Config*)persistent_storage->getSection(PersistentStorageAllocations::WISC_INFLUXDB_CONFIG, InfluxDB::ConfigVersion, sizeof(Config));
	configASSERT(this->config);

	if (psver == 0) {
		// No previous configuration, apply defaults
		this->setConfig(config);
		persistent_storage->flush(this->config, sizeof(Config));
	} else {
		this->flush_ticks = pdMS_TO_TICKS(this->config->flushInterval * 1000);
	}

	runTask(thread_name, TASK_PRIORITY_BACKGROUND, [this]() -> void {
		this->backgroundTask();
	});
}

InfluxDB::~InfluxDB() {
	vSemaphoreDelete(this->collector_mutex);
	vSemaphoreDelete(this->flush_mutex);
}

void InfluxDB::setConfig(const Config &config) {
	MutexGuard<false> lock(this->flush_mutex, true);

	CriticalGuard critical(true);
	memcpy(this->config, &config, sizeof(Config));
	critical.release();

	this->flush_ticks = pdMS_TO_TICKS(this->config->flushInterval * 1000);
}

InfluxDB::Timestamp InfluxDB::getCurrentTimestamp() {
	const auto now = high_resolution_clock::now();
	auto ns = duration_cast<nanoseconds>(now.time_since_epoch()).count();
	// Check if the time is after Jan 1st 2018
	if (ns < 0x1505868F5E440000) return 0;
	else return ns;
}

bool InfluxDB::write(const std::string& measurement, const TagSet& tags, const FieldSet& fields, Timestamp timestamp) {
	// Check inputs
	if (measurement.length() == 0 || fields.size() == 0) return false;

	// Can potentially be improved to save memory
	const Metric metric = {measurement, tags, fields, timestamp};
	{ // Mutexed area
		MutexGuard<false> lock(this->collector_mutex, true);
		this->collector->push_back(metric);
	}

	// Write was successful
	return true;
}

void InfluxDB::backgroundTask() {
	while (1) {
		vTaskDelay(this->flush_ticks);

		std::unique_ptr<MetricSet> metrics = nullptr;

		{ // Mutexed scope, swap the metrics block
			MutexGuard<false> lock(this->collector_mutex, true);

			if (this->collector->size() > 0) {
				metrics = std::move(this->collector);
				this->collector = std::unique_ptr<MetricSet>(new MetricSet());
				if (!this->collector) {
					logtree.log("Alloc failed, low memory? Terminating task", LogTree::LOG_ERROR);
					break; // No memory, stop the thread
				}
			} else {
				continue;
			}
		}

		// Push metrics to the database
		if (!this->push(*metrics)) {
			// If there is a problem this message will get spammed, we don't want that
			//this->logtree.log("Failed to push metrics to database", LogTree::LOG_ERROR);
			this->dropped_metrics += metrics->size();
		} else {
			this->pushed_metrics += metrics->size();
		}
	}
}

bool InfluxDB::push(const MetricSet& metrics) {
	MutexGuard<false> lock(this->flush_mutex, true);

	// Check set validity
	if (metrics.size() == 0) return false;

	// Check if there is a target database set
	if (this->config->host[0] == '\0') {
		logtree.log("Attempting to push metrics with no target configuration", LogTree::LOG_WARNING);
		return false;
	}

	std::unique_ptr<ClientSocket> socket;

	const std::string serverURL = std::string(this->config->host) + ":" + std::to_string(this->config->port);

	// No connection to server yet, attempt to connect
	try {
		socket = std::unique_ptr<ClientSocket>(new ClientSocket(this->config->host, this->config->port));
	} catch (const except::host_not_found& e) {
		logtree.log("Failed to get DNS entry for host " + serverURL, LogTree::LOG_DIAGNOSTIC);
		return false;
	}

	if (socket->connect() < 0) {
		// Failed to connect
		logtree.log("Failed to connect to host " + serverURL, LogTree::LOG_DIAGNOSTIC);
		return false;
	}

	// TODO: Consider changing to std::stringstream which is faster?

	// Generate the post message content
	std::string contents = "";

	for (const Metric& metric : metrics) {
		contents += metric.measurement;

		// Add tags, if any
		if (metric.tags.size() > 0) {
			for (const auto &tag : metric.tags) {
				contents += "," + tag.first + "=" + tag.second;
			}
		}
		contents += " ";

		// Add fields
		for (const auto &field : metric.fields) {
			contents += field.first + "=" + field.second + ",";
		}
		contents.pop_back();

		// Add timestamp
		// If 0 then use servers timestamp by not specifying a timestamp
		if (metric.timestamp != 0) {
			contents += " " + std::to_string(metric.timestamp);
		}

		// Terminate
		contents += "\n";
	}
	contents.pop_back(); // Remove last added '\n'

	// Generate the post request
	std::string post = "POST http://" + serverURL + "/write?db=" + this->config->database + " HTTP/1.0\r\n";
	post += "Content-Length: " + std::to_string(contents.length()) + "\r\n";
	post += "\r\n";
	post += contents;

	// In case debugging is required
	//logtree.log(post, LogTree::LOG_NOTICE);

	// Send the request
	socket->send(post);

	// Receive the response
	char resp[512];
	size_t resp_len = socket->recvn(resp, 512);
	if (resp_len < 0) {
		logtree.log("Failed to read response from socket", LogTree::LOG_ERROR);
		return false;
	} else if (resp_len == 512) {
		logtree.log("Error storing complete response from socket", LogTree::LOG_ERROR);
		return false;
	} else {
		// Good response, TODO do something if failed
	}

	// Write was successful
	return true;
}

//! A "influxdb.status" console command.
class InfluxDB::StatusCommand : public CommandParser::Command {
public:
	StatusCommand(InfluxDB &influxdb) : influxdb(influxdb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Prints current InfluxDB status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write("Measurements: " + std::to_string(influxdb.pushed_metrics) + " pushed / " + std::to_string(influxdb.dropped_metrics) + " dropped\n");
	}

private:
	InfluxDB &influxdb;
};

//! A "influxdb.config" console command.
class InfluxDB::ConfigCommand : public CommandParser::Command {
public:
	ConfigCommand(InfluxDB &influxdb) : influxdb(influxdb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Changes or shows InfluxDB client configuration. Usage:\n" +
				command + " database-url port database-name flush-interval-s\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			// Show configurations
			const InfluxDB::Config *config = influxdb.getConfig();
			console->write("Current configuration:\n");
			console->write("Host: " + std::string(config->host) + ":" + std::to_string(config->port) + '\n');
			console->write("Database: " + std::string(config->database) + '\n');
			console->write("Flush interval: " + std::to_string(config->flushInterval) + " seconds\n");
		} else {
			// Set configuration
			InfluxDB::Config config = {0};
			std::string host, database;

			if (!parameters.parseParameters(1, true, &host, &(config.port), &database, &(config.flushInterval))) {
				console->write("Invalid parameters. See help.\n");
				return;
			}

			if (host.length() >= 64) {
				console->write("Database URL is too long, max 63 characters allowed.\n");
				return;
			}

			if (database.length() >= 64) {
				console->write("Database name is too long, max 63 characters allowed.\n");
				return;
			}

			config.host = host;
			config.database = database;
			influxdb.setConfig(config);
		}
	}

private:
	InfluxDB &influxdb;
};

void InfluxDB::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "status", std::make_shared<InfluxDB::StatusCommand>(*this));
	parser.registerCommand(prefix + "config", std::make_shared<InfluxDB::ConfigCommand>(*this));
}

void InfluxDB::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "status", nullptr);
	parser.registerCommand(prefix + "config", nullptr);
}

