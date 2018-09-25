/*
 * InfluxDB.cpp
 *
 *  Created on: Sep 12, 2018
 *      Author: mpv
 */

// TODO: Consider implementing retries if failed to push metrics

#include "InfluxDB.h"
#include <semphr.h>
#include <task.h>
#include <chrono>
#include <services/persistentstorage/PersistentStorage.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>
#include <libs/ThreadingPrimitives.h>

using namespace std::chrono;

InfluxDB::InfluxDB(LogTree &logtree) :
logtree(logtree) {
	this->collectorMutex = xSemaphoreCreateMutex();
	this->flushMutex = xSemaphoreCreateMutex();
	this->collector = std::unique_ptr<MetricSet>(new MetricSet());
	configASSERT(this->collectorMutex && this->flushMutex && this->collector)

	// Load configuration
	uint16_t psver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_INFLUXDB_CONFIG);
	if (psver > 0 && psver != InfluxDB::ConfigVersion) {
		// Record exists but it is old, erase it.
		logtree.log("Out-dated configuration record, replacing with defaults.", LogTree::LOG_WARNING);
		persistent_storage->delete_section(PersistentStorageAllocations::WISC_INFLUXDB_CONFIG);
		psver = 0;
	}

	this->config = (Config*)persistent_storage->get_section(PersistentStorageAllocations::WISC_INFLUXDB_CONFIG, InfluxDB::ConfigVersion, sizeof(Config));
	configASSERT(this->config);

	if (psver == 0) {
		// No previous configuration, apply defaults
		this->setConfig(InfluxDB::defaultConfig);
		persistent_storage->flush(this->config, sizeof(Config));
	} else {
		this->flushTicks = pdMS_TO_TICKS(this->config->flushInterval * 1000);
	}

	configASSERT(UWTaskCreate("influxdbd", TASK_PRIORITY_BACKGROUND, [this]() -> void {
		this->_backgroundTask();
	}));
}

InfluxDB::~InfluxDB() {
	vSemaphoreDelete(this->collectorMutex);
	vSemaphoreDelete(this->flushMutex);
}

void InfluxDB::setConfig(const Config &config) {
	MutexLock lock(this->flushMutex);
	taskENTER_CRITICAL();
	memcpy(this->config, &config, sizeof(Config));
	taskEXIT_CRITICAL();

	this->flushTicks = pdMS_TO_TICKS(this->config->flushInterval * 1000);
}

InfluxDB::Timestamp InfluxDB::getCurrentTimestamp() {
	const auto now = high_resolution_clock::now();
	auto ns = duration_cast<nanoseconds>(now.time_since_epoch()).count();
	// Check if the time is after Jan 1st 2018
	if (ns < 0x1505868F5E440000) return 0;
	else return ns;
}

bool InfluxDB::write(const std::string& measurement, const TagSet& tags, const FieldSet& fields, long long timestamp) {
	// Check inputs
	if (measurement.length() == 0 || fields.size() == 0) return false;

	// Can potentially be improved to save memory
	const Metric metric = {measurement, tags, fields, timestamp};
	{ // Mutexed area
		MutexLock lock(this->collectorMutex);
		this->collector->push_back(metric);
	}

	// Write was successful
	return true;
}

void InfluxDB::_backgroundTask() {
	while (1) {
		vTaskDelay(this->flushTicks);

		std::unique_ptr<MetricSet> metrics = nullptr;

		{ // Mutexed scope, swap the metrics block
			MutexLock lock(this->collectorMutex);

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
#ifdef INFLUXDB_STATS
			this->droppedMeasurements += metrics->size();
#endif
		} else {
#ifdef INFLUXDB_STATS
			this->pushedMeasurements += metrics->size();
#endif
		}
	}
}

bool InfluxDB::push(const MetricSet& metrics) {
	MutexLock lock(this->flushMutex);

	// Check inputs
	if (metrics.size() == 0) return false;

	std::unique_ptr<ClientSocket> socket;

	const std::string serverURL = std::string(this->config->host) + ":" + std::to_string(this->config->port);

	// No connection to server yet, attempt to connect
	try {
		socket = std::unique_ptr<ClientSocket>(new ClientSocket(this->config->host, this->config->port));
	} catch (const SocketAddress::HostNotFound& e) {
		logtree.log("Failed to get DNS entry for host " + serverURL, LogTree::LOG_DIAGNOSTIC);
		return false;
	}

	if (socket->connect() < 0) {
		// Failed to connect
		logtree.log("Failed to connect to host " + serverURL, LogTree::LOG_DIAGNOSTIC);
		return false;
	}

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

#ifdef INFLUXDB_STATS
/// A "influxdb.status" console command.
class influxdb_status : public CommandParser::Command {
public:
	InfluxDB &influxdb; ///< InfluxDB object.

	/// Instantiate
	influxdb_status(InfluxDB &influxdb) : influxdb(influxdb) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Prints current InfluxDB status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write("Measurements: " + std::to_string(influxdb.pushedMeasurements) + " pushed / " + std::to_string(influxdb.droppedMeasurements) + " dropped\n");
	}
};
#endif

/// A "influxdb.config" console command.
class influxdb_config : public CommandParser::Command {
public:
	InfluxDB &influxdb; ///< InfluxDB object.

	/// Instantiate
	influxdb_config(InfluxDB &influxdb) : influxdb(influxdb) { };

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

			if (!parameters.parse_parameters(1, true, &host, &(config.port), &database, &(config.flushInterval))) {
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

			strcpy(config.host, host.c_str());
			strcpy(config.database, database.c_str());

			influxdb.setConfig(config);
		}
	}
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDB::register_console_commands(CommandParser &parser, const std::string &prefix) {
#ifdef INFLUXDB_STATS
	parser.register_command(prefix + "status", std::make_shared<influxdb_status>(*this));
#endif
	parser.register_command(prefix + "config", std::make_shared<influxdb_config>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDB::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
#ifdef INFLUXDB_STATS
	parser.register_command(prefix + "status", NULL);
#endif
	parser.register_command(prefix + "config", NULL);
}

