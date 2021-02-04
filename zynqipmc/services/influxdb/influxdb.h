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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_INFLUXDB_INFLUXDB_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_INFLUXDB_INFLUXDB_H_


#include "FreeRTOS.h"
#include "semphr.h"
#include "xil_types.h"
#include <string>
#include <vector>
#include <drivers/network/client_socket.h>
#include <libs/logtree/logtree.h>
#include <libs/statcounter/statcounter.h>
#include <services/console/command_parser.h>
#include <services/persistentstorage/persistent_storage.h>

// TODO: Add a field type that can accept int, float, string and bool. Right now only strings are accepted.

/**
 * InfluxDB client which allows to send sensor data to a remove server.
 */
class InfluxDB final : public ConsoleCommandSupport {
public:

	static const size_t maxStringLen = 65536;       ///< Maximum string length within a field or tag that influxdb can handle

        static const size_t maxConfigHostName = 63;     ///< Maximum string length of host name in config
        static const size_t maxConfigDatabaseName = 63; ///< Maximum string length of database name in config
    	static const size_t maxFlushInterval = 60;      ///< Maximum flushInterval (in seconds) - need some max to detect invalid config values

	//! Configuration structure.
	using Config = struct {
		char host[maxConfigHostName+1]; 	///< The host name where InfluxDB server resides (+1 for null)
		uint16_t port;  	  	        ///< The TCP/IP port to connect to.
		char database[maxConfigDatabaseName+1];	///< The database name. (+1 for null)
		uint16_t flushInterval;                 ///< Interval period in seconds between pushes to server.
	};
	static const int ConfigVersion = 4; ///< Configuration version, for persistent storage.
        static const uint16_t ConfigSection = PersistentStorageAllocations::WISC_INFLUXDB_CONFIG;  ///< Section ID, for persistent storage.

        static const Config configDefault;

	/**
	 * Constructor, automatically starts the flush thread.
	 * @param persistent_storage object of Class PersistentStorage to use for retrieving/storing board serial number
	 * @param logtree Log tree to use for logging.
	 * @param config Client configuration structure, can also be set using the config command.
	 * @param thread_name Name to associate to the background thread.
	 */
    InfluxDB(PersistentStorage &persistent_storage, LogTree &logtree, const Config& config = InfluxDB::configDefault, const std::string& thread_name = "influxdb");
	virtual ~InfluxDB();

	//! InfluxDB timestamp.
	using Timestamp = long long;
	//! Tag set, used to specify the tags of a measurement. Pairs should be <tag_name, tag_value>.
	using TagSet = std::vector<std::pair<std::string, std::string>>;
	//! Field set, used to specify the fields in a measurement. Pairs should be <field_name, field_value>.
	using FieldSet = std::vector<std::pair<std::string, std::string>>;

	//! A single measurement/metric.
	struct Metric {
		std::string measurement;	///< Measurement name.
		TagSet tags;				///< All associated tags.
		FieldSet fields;			///< All associated fields.
		Timestamp timestamp;		///< Timestamp, in nano-seconds (ms).
	};

	//! A set of measurements, used internally.
	using MetricSet = std::vector<Metric>;

	/**
	 * Converts ticks into a timestamp in nano-seconds. Can be used in InfluxDB::write.
	 * If system time is not set (e.g. by NTP), this will return the tick value as Timestamp in nano-seconds since start of system
	 *
	 * @param ticks absolute time stamp in system ticks (ticks since start) to be converted to ns from epoch
	 */
	static Timestamp ticksTimestamp(const uint64_t ticks);

	/**
	 * Returns the current timestamp in nano-seconds. Can be used in InfluxDB::write.
	 * Will return 0 if system time is not set (e.g. by NTP).
	 */
	static Timestamp getCurrentTimestamp();

	/**
	 * Write a measurement to the server. Write won't happen until a flush takes place.
	 * @param measurement The name of the measurement.
	 * @param tags The associated set of tags.
	 * @param fields The associated fields.
	 * @param timeout Mutex timeout in ticks. This prevents the calling task from getting blocked forever unless portMAX_DELAY.
	 * @param timestamp Timestamp in nano-seconds. Optional, current timestamp is automatically added.
	 * @return true if successful, false otherwise.
	 * @note Special care must be taken when deciding what should be a measurement, a tag or a field.
	 * 1. Measurement is a unique identifier, the container of several fields and tags. Similar to a table in SQL.
	 * 2. Tags are indexed, hashed and are normally used when searching.
	 * 3. Fields are NOT indexed and should be the actual metrics associated with a measurement. Math can easily be done with fields.
	 */
	bool write(const std::string& measurement, const TagSet& tags, const FieldSet& fields, const TickType_t timeout = portMAX_DELAY, const Timestamp timestamp = getCurrentTimestamp());

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	// Console commands
	class StatusCommand;
	class ConfigCommand;

	Config config;							///< InfluxDB configuration.

	PersistentStorage &persistent_storage; ///< Persistent Storage
	LogTree &logtree;						///< Log target.

	//! Collector used to aggregate several metrics before pushing them to remote server.
	std::unique_ptr<MetricSet> collector;
	SemaphoreHandle_t collector_mutex;		///< Mutex dedicated for the collector.

	TickType_t flush_ticks;				///< Number of FreeRTOS ticks between automatic flushes.
	SemaphoreHandle_t flush_mutex;			///< Mutex used to lock the flush transaction.

	SemaphoreHandle_t config_mutex;			///< Mutex used to lock config access

	StatCounter push_enabled;	    ///< 1 if push to database is enabled, else 0
	StatCounter pushed_metrics;     ///< Total number of pushed metrics.
	StatCounter dropped_metrics;	///< Total number of dropped metrics.

	//! Background task to push metrics automatically.
	void backgroundTask();

	//! Push metric set to the server.
	bool push(const MetricSet& metrics);

	//! Old Configuration structure.
	using ConfigV3 = struct {
	        std::string host;                 	///< The host name where InfluxDB server resides
		uint16_t port;  	  	        ///< The TCP/IP port to connect to.
	        std::string database;           	///< The database name.
		uint16_t flushInterval;                 ///< Interval period in seconds between pushes to server.
	};

	/**
	 * Set and save the configuration. A special config V3 version is available.
	 * @param config The configuration to apply.
	 */
        void setConfig(const ConfigV3 &cfgV3);
	void setConfig(const Config &config);

	//! Get the current configuration.
	inline const Config getConfig() { MutexGuard<false> lock(this->config_mutex, true); return this->config; };

	/**
	 * Save configuration in persistent storage
	 * @return true if successful, false otherwise.
	 */
	bool storeConfig();

	/**
	 * Retrieve configuration from persistent storage
	 * @return true if successful, false otherwise.
	 */
	bool retrieveConfig();


	/**
	 * Save configuration in persistent storage using old version 3 format
	 * @return true if successful, false otherwise.
	 */
	bool storeConfigVer3();

	/**
	 * Retrieve configuration from persistent storage using old version 3 format
	 * @return true if successful, false otherwise.
	 */
	bool retrieveConfigVer3();

        /**
	 * Retrieve configuration from persistent storage using the current version format
	 * @return true if successful, false otherwise.
	 */
	bool retrieveConfigLatest();

	//! Return enable (true)/disable (false) state of influxdbclient
	//! Since looking at flush_time, can return it under the same mutex if pFlushTicks is used.
	//! No external class needs to know this. They just call write() and it will all be taken care of.
	inline const bool isEnabled(TickType_t *pFlushTicks = nullptr) {
		MutexGuard<false> lock(this->config_mutex, true);
		if (pFlushTicks) *pFlushTicks = this->flush_ticks;
		return ((this->flush_ticks >= pdMS_TO_TICKS(1 * 1000))
				&& (this->flush_ticks <= pdMS_TO_TICKS(maxFlushInterval * 1000))
				&& (this->config.port != 0));
	};

};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_INFLUXDB_INFLUXDB_H_ */
