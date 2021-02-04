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
#include <core_commands/date.inc>

using namespace std::chrono;

const InfluxDB::Config InfluxDB::configDefault = {"", 0, "", 0};

InfluxDB::InfluxDB(PersistentStorage &persistent_storage, LogTree &logtree, const Config& config, const std::string& thread_name)
    : persistent_storage(persistent_storage), logtree(logtree), flush_ticks(0),
	  push_enabled("influxdb.push_enabled"),
	  pushed_metrics("influxdb.total_metrics_pushed"),
	  dropped_metrics("influxdb.total_metrics_dropped") {
	this->collector_mutex = xSemaphoreCreateMutex();
	this->flush_mutex = xSemaphoreCreateMutex();
	this->config_mutex = xSemaphoreCreateMutex();
	this->collector = std::unique_ptr<MetricSet>(new MetricSet());
	configASSERT(this->collector_mutex && this->flush_mutex && this->config_mutex && this->collector)

	// Load configuration
	if (!this->retrieveConfig()) {
	    // retrieve of config from persistent storage failed, so use passed-in defaults
	    this->setConfig(config);
	    // Update persistent storage so will use this on next boot
	    this->storeConfig();	    
	}

	runTask(thread_name, TASK_PRIORITY_BACKGROUND, [this]() -> void {
		this->backgroundTask();
	});
}

InfluxDB::~InfluxDB() {
	vSemaphoreDelete(this->collector_mutex);
	vSemaphoreDelete(this->flush_mutex);
	vSemaphoreDelete(this->config_mutex);
}

void InfluxDB::setConfig(const Config &config) {
	MutexGuard<false> lock(this->config_mutex, true);

	CriticalGuard critical(true);
	this->config = config;
	critical.release();

	this->flush_ticks = pdMS_TO_TICKS(this->config.flushInterval * 1000);

	// When config gets set, update the push_enabled stat. Need to
	// release lock first since isEnabled() locks it.
	lock.release();
	push_enabled.set(this->isEnabled() ? 1 : 0);

}

void InfluxDB::setConfig(const ConfigV3 &cfgV3) {
	Config configNew;
	
	strncpy(configNew.host, cfgV3.host.c_str(), InfluxDB::maxConfigHostName);
	configNew.host[InfluxDB::maxConfigHostName] = '\0'; // make sure it is null terminated

	configNew.port          = cfgV3.port;

	strncpy(configNew.database, cfgV3.database.c_str(), InfluxDB::maxConfigDatabaseName);
	configNew.database[InfluxDB::maxConfigDatabaseName] = '\0'; // make sure it is null terminated

	configNew.flushInterval = cfgV3.flushInterval;

	this->setConfig(configNew);
}


bool InfluxDB::storeConfigVer3() {
	const InfluxDB::Config cfgNew = this->getConfig();
	InfluxDB::ConfigV3 cfgV3;
	cfgV3.host          = cfgNew.host;
	cfgV3.port          = cfgNew.port;
	cfgV3.database      = cfgNew.database;
	cfgV3.flushInterval = cfgNew.flushInterval;

	MutexGuard<false> lock(this->config_mutex, true);

	// Build byte form for storing in persistent storage and Add up size of config. Since it uses std::string, must convert to simple bytes
	const size_t szConfig =  cfgV3.host.length() + 1 + sizeof(cfgV3.port) + cfgV3.database.length() + 1 + sizeof(cfgV3.flushInterval);
	uint8_t *pdata = nullptr;
	
	// Assume that size of strings changed, so will always reallocate. 
	this->persistent_storage.deleteSection(InfluxDB::ConfigSection);
	pdata = (uint8_t*) this->persistent_storage.getSection(InfluxDB::ConfigSection, 3, szConfig);
	if (!pdata) {
	    logtree.log("Allocation of persistent storage for influxdb config failed! ", LogTree::LOG_ERROR);
	    return false; // Allocation failed?
	}

	// Now, write the data
	uint8_t *pdataptr = pdata;

	// Copy host
	strcpy((char*)pdataptr, cfgV3.host.c_str());
	pdataptr += cfgV3.host.length() + 1; // +1 for null terminator

	// Copy port, lsb first
	*(pdataptr++) = (uint8_t)(cfgV3.port & 0x00ff);
	*(pdataptr++) = (uint8_t)((cfgV3.port >> 8) & 0x00ff);

	// Copy database
	strcpy((char*)pdataptr, cfgV3.database.c_str());
	pdataptr += cfgV3.database.length() + 1; // +1 for null terminator

	// Copy flushInterval, lsb first
	*(pdataptr++) = (uint8_t)(cfgV3.flushInterval & 0x00ff);
	*(pdataptr++) = (uint8_t)((cfgV3.flushInterval >> 8) & 0x00ff);

	this->persistent_storage.flush(pdata, szConfig);
	return true;
}

bool InfluxDB::storeConfig() {

        const InfluxDB::Config cfg = this->getConfig();

	MutexGuard<false> lock(this->config_mutex, true);

	// Build byte form for storing in persistent storage and Add up size of config. Since it uses std::string, must convert to simple bytes
	const size_t szConfig = sizeof(cfg);
	uint8_t *pdata = nullptr;
	
	// NOTE: Size of strings is a constant allocation, so no need to delete section each time, like done for Ver3
	pdata = (uint8_t*) this->persistent_storage.getSection(InfluxDB::ConfigSection, InfluxDB::ConfigVersion, szConfig);
	if (!pdata) {
	    logtree.log("Allocation of persistent storage for influxdb config failed! ", LogTree::LOG_ERROR);
	    return false; // Allocation failed?
	}

	// Now, write the data
	memcpy(pdata, &cfg, szConfig);
	this->persistent_storage.flush(pdata, szConfig);

	return true;
}

bool InfluxDB::retrieveConfigVer3() {
    // This code appears to be flawed - where does the length() of
    // host and database come from before they are defined?  Will keep
    // code around, which appears to work, for the purpose to
    // automatically upgrading to Ver 4.
    
    ConfigV3 config;

    {
	MutexGuard<false> lock(this->config_mutex, true);

	// Build byte form for retrieving from persistent storage and Add up size of config. Since it uses std::string, must convert to simple bytes
	const size_t szConfig =  config.host.length() + 1 + sizeof(config.port) + config.database.length() + 1 + sizeof(config.flushInterval);
	uint8_t *pdata = nullptr;
	
	logtree.log(stdsprintf("Reading influxdb configuration record of size %u from persistent storage", szConfig), LogTree::LOG_NOTICE);

	pdata = (uint8_t*) this->persistent_storage.getSection(InfluxDB::ConfigSection, 3, szConfig);
	if (!pdata) {
	    logtree.log("Fetch of influxdb config from persistent storage failed! ", LogTree::LOG_ERROR);
	    return false;
	}

	logtree.log("Reading influxdb configuration record from persistent storage - successful", LogTree::LOG_INFO);
	
	// Now, read the data
	uint8_t *pdataptr = pdata;

	config.host = (char*)pdataptr;
	pdataptr += config.host.length() + 1; // +1 for null terminator

	config.port = *(pdataptr++);
	config.port |= (((uint16_t)*(pdataptr++)) << 8);

	config.database = (char*)pdataptr;
	pdataptr += config.database.length() + 1; // +1 for null terminator

	config.flushInterval = *(pdataptr++);
	config.flushInterval |= (((uint16_t)*(pdataptr++)) << 8);
    }

    // set the retrieved config
    setConfig(config);
    return true;
}

bool InfluxDB::retrieveConfigLatest() {

    Config config;

    {
	MutexGuard<false> lock(this->config_mutex, true);

	const size_t szConfig = sizeof(this->config);
	uint8_t *pdata = nullptr;
	
	pdata = (uint8_t*) this->persistent_storage.getSection(InfluxDB::ConfigSection, InfluxDB::ConfigVersion, szConfig);
	if (!pdata) {
	    logtree.log("Fetch of influxdb config from persistent storage failed! ", LogTree::LOG_ERROR);
	    return false;
	}

	logtree.log("Reading influxdb configuration record from persistent storage", LogTree::LOG_NOTICE);
	
	// Now, read the data into config
	memcpy(&config, pdata, szConfig);
    }

    // set the retrieved config
    setConfig(config);
    return true;
}

bool InfluxDB::retrieveConfig() {

    ConfigV3 config;

    {
	MutexGuard<false> lock(this->config_mutex, true);

	// Attempt to fetch config from persistent storage and put into this object's config
	uint16_t psver = this->persistent_storage.getSectionVersion(InfluxDB::ConfigSection);

	// If it is found and old V3, then handle upgrade to V4 for user
	if (psver == 3) {
	    lock.release();
	    logtree.log("Old V3 version of influxdb configuration found. Upgrading to latest configuration format.", LogTree::LOG_NOTICE);
	    if (this->retrieveConfigVer3()) {
		this->persistent_storage.deleteSection(InfluxDB::ConfigSection);
		return this->storeConfig();
	    } else {
		// return that retrieveConfigVer3() failed
		return false;
	    }
	} else if (psver > 0 && psver != InfluxDB::ConfigVersion) {
		// Record exists but it is old, erase it.
		logtree.log("Deleting Out-dated influxdb configuration record", LogTree::LOG_WARNING);
		this->persistent_storage.deleteSection(InfluxDB::ConfigSection);
		return false;
	} else if (psver == 0) {
	       // could not find configuration in persistent storage - return false
	       return false;
	}

    }

    // If reach here, influxdb config with correct version must have been located, so retrieve it
    return this->retrieveConfigLatest();
    
}

InfluxDB::Timestamp InfluxDB::ticksTimestamp(const uint64_t ticks) {
	auto then = high_resolution_clock::now();
	const std::chrono::duration<int,std::ratio<1, configTICK_RATE_HZ>> duration(get_tick64() - ticks);
	then -= duration;			// then: now() - duration
	auto ns = duration_cast<nanoseconds>(then.time_since_epoch()).count();
	return ns;
}

InfluxDB::Timestamp InfluxDB::getCurrentTimestamp() {
	const auto now = high_resolution_clock::now();
	auto ns = duration_cast<nanoseconds>(now.time_since_epoch()).count();
	// Check if the time is after Jan 1st 2018
	if (ns < 0x1505868F5E440000) return 0;
	else return ns;
}

bool InfluxDB::write(const std::string& measurement, const TagSet& tags, const FieldSet& fields, const TickType_t timeout, const Timestamp timestamp) {
	// Check inputs
	if (measurement.length() == 0 || fields.size() == 0) return false;

	// Set port to 0 or flush_interval to 0 to disable pushing to influxdb
	if (!this->isEnabled()) {
		// NOTE: Avoid use of log() here so do not get recursion through influxdb_log_handler()
		//@@@logtree.log("Influxdb disabled so no data push. Ignoring the write()", LogTree::LOG_DIAGNOSTIC);
		return true;	// act like transfer worked
	}

	// NOTE: Avoid use of log() here so do not get recursion through influxdb_log_handler()
	//@@@logtree.log("Writing " + measurement + " at " + std::to_string(timestamp), LogTree::LOG_TRACE);

	// Can potentially be improved to save memory but don't be
	// tempted to make metric be Metric* pointer because that
	// seems to cause the tasks that call this to balloon in CPU
	// usage. It is as if that causes the task that calls this to
	// be blocked longer by collector_mutex somehow.
	//
	// Also, forming metric before entering the Mutexed area
	// minimizes how long in the Mutexed area.
	const Metric metric = {measurement, tags, fields, timestamp};
	try { // Mutexed area
		MutexGuard<false> lock(this->collector_mutex, true, pdMS_TO_TICKS(timeout));
		this->collector->push_back(metric);
		//@@@ if (strcmp("i2c_sensors",pcTaskGetName( NULL )) == 0) vTaskDelay(pdMS_TO_TICKS(12)); //@@@ Test the timeout_error exception code
	}
	catch (except::timeout_error &e) {
		// Mutex timed out
		//
		// Only output a log if this is NOT the Influxdb log handler. Do not want the log handler to recursively triggering itself if there is a time-out.
		if (measurement != "Logs") logtree.log(stdsprintf("InfluxDB write() in task '%s' timed out: %s", pcTaskGetName( NULL ), e.what()), LogTree::LOG_WARNING);
		return false;
	}

	// Write was successful
	return true;
}

void InfluxDB::backgroundTask() {
	while (1) {
		TickType_t flushTicks;
		if (this->isEnabled(&flushTicks)) {
			// NOTE: isEnabled() checks that flush_ticks is
			// valid, so this check also prevents in invalid
			// amount of time getting into vTaskDelay()
			vTaskDelay(flushTicks);
		} else {
			// If influxdbclient is disabled, still wait
			// an infrequent amount of time and then clean
			// out metrics in case some other task is
			// still passing in data.
			vTaskDelay(pdMS_TO_TICKS(12000));
			logtree.log("Influxdb is disabled", LogTree::LOG_DIAGNOSTIC);
		}

		logtree.log("InfluxDB Collection running at: " + CoreCommands::DateCommand::getString(), LogTree::LOG_INFO);

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
				if (!this->isEnabled()) {
				    // if not enabled, do not bother trying to push anything
				    continue;
				}
			} else {
			        logtree.log("No metrics to send, so skipping push of metrics to database", LogTree::LOG_DIAGNOSTIC);
				continue;
			}
		}

		// Push metrics to the database
		if (!this->push(*metrics)) {
			// If there is a problem this message will get spammed, we don't want that
			//this->logtree.log("Failed to push metrics to database", LogTree::LOG_ERROR);
			this->dropped_metrics.increment(metrics->size());
		} else {
			this->pushed_metrics.increment(metrics->size());
		}
	}
}

bool InfluxDB::push(const MetricSet& metrics) {
	const InfluxDB::Config cfg = this->getConfig();
	MutexGuard<false> lock(this->flush_mutex, true);

	// Check set validity
	if (metrics.size() == 0) return false;

	// Set port to 0 or flush_interval to 0 to disable pushing to influxdb
	if (!this->isEnabled()) {
		logtree.log("Influxdb disabled so no data push", LogTree::LOG_DIAGNOSTIC);
		return false;
	}

	// Check if there is a target database set
	if (cfg.host[0] == '\0') {
		logtree.log("Attempting to push metrics to influxdb with no target configuration", LogTree::LOG_WARNING);
		return false;
	}

	std::unique_ptr<ClientSocket> socket;

	const std::string serverURL = std::string(cfg.host) + ":" + std::to_string(cfg.port);

	logtree.log("Pushing to " + serverURL, LogTree::LOG_DIAGNOSTIC);

	// No connection to server yet, attempt to connect
	try {
		socket = std::unique_ptr<ClientSocket>(new ClientSocket(cfg.host, cfg.port));
	} catch (const except::host_not_found& e) {
		logtree.log("Host not found " + serverURL, LogTree::LOG_DIAGNOSTIC);
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
	    // @@@ For debug, do not send Sensor data
	    //@@@if (metric.measurement == "Sensors") {
	    //@@@continue;
	    //@@@} else {
		contents += metric.measurement;

		// Add tags, if any
		if (metric.tags.size() > 0) {
		        // influxdb suggests sorting tags but the database host can likely do it faster than I
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
		//@@@}
	}

	if (contents.length() == 0) {
	    logtree.log("Contents empty, so skipping push to influxdb", LogTree::LOG_INFO);
	    return false;
	}
	    
	contents.pop_back(); // Remove last added '\n'

	// Generate the post request
	std::string post = "POST http://" + serverURL + "/write?db=" + std::string(cfg.database) + " HTTP/1.0\r\n";
	post += "Content-Length: " + std::to_string(contents.length()) + "\r\n";
	post += "\r\n";
	post += contents;

	// In case debugging is required
	//@@@ logtree.log(post, LogTree::LOG_DIAGNOSTIC); //@@@

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
		logtree.log(resp, LogTree::LOG_INFO); 
		return false;
	} else {
		// Good response, TODO do something if failed
 	        //@@@logtree.log(resp, LogTree::LOG_DIAGNOSTIC); 
	}

	// Write was successful
	return true;
}

//! A "influxdb.status" console command.
class InfluxDB::StatusCommand : public CommandParser::Command {
public:
	StatusCommand(InfluxDB &influxdb) : influxdb(influxdb) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Prints current InfluxDB status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write("Measurements: " + std::to_string(influxdb.pushed_metrics.get()) + " pushed / " + std::to_string(influxdb.dropped_metrics.get()) + " dropped"
			       + "   Client is: " + (influxdb.push_enabled.get() ? "Enabled" : "Disabled") + "\n");
	}

private:
	InfluxDB &influxdb;
};

//! A "influxdb.config" console command.
class InfluxDB::ConfigCommand : public CommandParser::Command {
public:
    ConfigCommand(InfluxDB &influxdb) : influxdb(influxdb) { };

    virtual std::string getHelpText(const std::string &command) const {
	return command + " $database-ip-addr $port $database-name $flush-interval-secs\n\n"
	    + "Without parameters, shows InfluxDB client configuration. With parameters, changes the configuration.\n"
		+ "Set $port or $flush-interval-secs to 0 to disable sending of data to InfluxDB.\n";
    }

    virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
	//@@@for (auto prm : parameters.parameters) {
	//    console->write("Parameter: " + prm + "\n");
	//}			
	std::string consoleStr;

	if (parameters.nargs() == 1) {
	    // Show configurations
	    const InfluxDB::Config cfg = influxdb.getConfig();
	    consoleStr = "Current configuration:\n";
	    consoleStr += "Host: " + std::string(cfg.host) + ":" + std::to_string(cfg.port) + '\n';
	    consoleStr += "Database: " + std::string(cfg.database) + '\n';
	    consoleStr += "Flush interval: " + std::to_string(cfg.flushInterval) + " seconds\n";
	    console->write(consoleStr.c_str());
	} else if (parameters.nargs() <= 5) {
	    // Set configuration
	    InfluxDB::ConfigV3 cfg;
	    std::string host;
	    std::string database;

	    if (!parameters.parseParameters(1, true, &host, &cfg.port, &database, &cfg.flushInterval)) {
			console->write("Invalid parameters. Review Help Text:\n\n" + this->getHelpText(parameters.parameters[0]));
			return;
	    }

	    if (host.length() > influxdb.maxConfigHostName) {
			console->write(stdsprintf("Database URL is too long, max %u characters allowed.\n", influxdb.maxConfigHostName));
			return;
	    }

	    if (database.length() > influxdb.maxConfigDatabaseName) {
			console->write(stdsprintf("Database name is too long, max %u characters allowed.\n", influxdb.maxConfigDatabaseName));
			return;
	    }

	    if (cfg.flushInterval > influxdb.maxFlushInterval) {
			console->write(stdsprintf("Flush Interval is too long, max %u seconds allowed.\n", influxdb.maxFlushInterval));
			return;
	    }

	    cfg.host = host;
	    cfg.database = database;

	    consoleStr = "Update configuration to:\n";
	    consoleStr += "Host: " + cfg.host + ":" + std::to_string(cfg.port) + '\n';
	    consoleStr += "Database: " + cfg.database + '\n';
	    consoleStr += "Flush interval: " + std::to_string(cfg.flushInterval) + " seconds\n";
	    console->write(consoleStr.c_str());

	    influxdb.setConfig(cfg);

	    // Update persistent storage so will use this on next boot
	    //@@@influxdb.storeConfigVer3(); // Use this to test conversion of V3 to V4 config in persistent_storage
	    influxdb.storeConfig();

	} else {
	    console->write("Invalid parameters. Review Help Text:\n\n" + this->getHelpText(parameters.parameters[0]));
	    return;
	}
    }

private:
    InfluxDB &influxdb;
};

void InfluxDB::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
    //@@@ Using Statcounters so do not need special command - use 'stats'
	//@@@parser.registerCommand(prefix + "status", std::make_shared<InfluxDB::StatusCommand>(*this));
    parser.registerCommand(prefix + "config", std::make_shared<InfluxDB::ConfigCommand>(*this));
}

void InfluxDB::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
    //@@@ Using Statcounters so do not need special command - use 'stats'
	//@@@    parser.registerCommand(prefix + "status", nullptr);
    parser.registerCommand(prefix + "config", nullptr);
}

