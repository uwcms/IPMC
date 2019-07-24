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

#ifndef SRC_COMMON_UW_IPMC_SERVICES_INFLUXDB_INFLUXDB_H_
#define SRC_COMMON_UW_IPMC_SERVICES_INFLUXDB_INFLUXDB_H_

#include <string>
#include <FreeRTOS.h>
#include <semphr.h>
#include <drivers/network/client_socket.h>
#include <services/console/CommandParser.h>
#include <libs/logtree/logtree.h>

// TODO: Add a field type that can accept int, float, string and bool. Right now only strings are accepted.

/**
 * InfluxDB client which allows to send sensor data to a remove server.
 */
class InfluxDB final : public ConsoleCommandSupport {
public:
	//! Configuration structure.
	using Config = struct {
		std::string host;		///< The host name where InfluxDB server resides.
		uint16_t port;			///< The TCP/IP port to connect to.
		std::string database;	///< The database name.
		uint16_t flushInterval; ///< Interval period in seconds between pushes to server.
	};
	const int ConfigVersion = 2; ///< Configuration version, for persistent storage.

	/**
	 * Constructor, automatically starts the flush thread.
	 * @param logtree Log tree to use for logging.
	 * @param config Client configuration structure, can also be set using the config command.
	 * @param thread_name Name to associate to the background thread.
	 */
	InfluxDB(LogTree &logtree, const Config& config = {"", 0, "", 5}, const std::string& thread_name = "influxdb");
	~InfluxDB();

	/**
	 * Set and save the configuration.
	 * @param config The configuration to apply.
	 */
	void setConfig(const Config &config);

	//! Get the current configuration.
	inline const Config* getConfig() { return this->config; };

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
	 * Returns the current timestamp in nano-seconds. Can be used in InfluxDB::write.
	 * Will return 0 if system time is not set (e.g. by NTP).
	 */
	static Timestamp getCurrentTimestamp();

	/**
	 * Write a measurement to the server. Write won't happen until a flush takes place.
	 * @param measurement The name of the measurement.
	 * @param tags The associated set of tags.
	 * @param fields The associated fields.
	 * @param timestamp Timestamp in nano-seconds. Optional, current timestamp is automatically added.
	 * @return true if successful, false otherwise.
	 * @note Special care must be taken when deciding what should be a measurement, a tag or a field.
	 * 1. Measurement is a unique identifier, the container of several fields and tags. Similar to a table in SQL.
	 * 2. Tags are indexed, hashed and are normally used when searching.
	 * 3. Fields are NOT indexed and should be the actual metrics associated with a measurement. Math can easily be done with fields.
	 */
	bool write(const std::string& measurement, const TagSet& tags, const FieldSet& fields, Timestamp timestamp = getCurrentTimestamp());

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	// Console commands
	class StatusCommand;
	class ConfigCommand;

	Config *config;							///< InfluxDB configuration.
	LogTree &logtree;						///< Log target.

	//! Collector used to aggregate several metrics before pushing them to remote server.
	std::unique_ptr<MetricSet> collector;
	SemaphoreHandle_t collector_mutex;		///< Mutex dedicated for the collector.

	TickType_t flush_ticks;					///< Number of FreeRTOS ticks between automatic flushes.
	SemaphoreHandle_t flush_mutex;			///< Mutex used to lock the flush transaction.

	size_t pushed_metrics;	///< Total number of pushed metrics.
	size_t dropped_metrics;	///< Total number of dropped metrics.

	//! Background task to push metrics automatically.
	void backgroundTask();

	//! Push metric set to the server.
	bool push(const MetricSet& metrics);
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_INFLUXDB_INFLUXDB_H_ */
