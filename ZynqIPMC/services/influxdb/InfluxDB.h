/*
 * InfluxDB.h
 *
 *  Created on: Sep 12, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_INFLUXDB_INFLUXDB_H_
#define SRC_COMMON_UW_IPMC_SERVICES_INFLUXDB_INFLUXDB_H_

#include <drivers/network/client_socket.h>

#include <IPMC.h>
#include <FreeRTOS.h>
#include <memory>

// TODO: Add a field type that can accept int, float, string and bool. Right now only strings are accepted.

///! Uncomment to keep track of how many measurements were sent/dropped
#define INFLUXDB_STATS

/**
 * InfluxDB client which allows to send sensor data to a remove server.
 */
class InfluxDB {
public:
	///! Configuration structure.
	using Config = struct {
		char host[64];			///< The host name where InfluxDB server resides.
		uint16_t port;			///< The TCP/IP port to connect to.
		char database[64];		///< The database name.
		uint16_t flushInterval; ///< Interval period in second between pushes to server.
	};
	const int ConfigVersion = 2; ///< Configuration version, for persistent storage.

	///! Defaults configurations applied if the record doesn't exist.
	const Config defaultConfig = {
		"192.168.1.8",		// host
		8086,				// port
		"ipmc",				// database
		5					// flush interval
	};

	/**
	 * Constructor, automatically starts the flush thread.
	 * @param logtree Log tree to use for logging.
	 */
	InfluxDB(LogTree &logtree);
	~InfluxDB();

	/**
	 * Set and save the configuration.
	 * @param config The configuration to apply.
	 */
	void setConfig(const Config &config);
	///! Get the current configuration.
	inline const Config* getConfig() { return this->config; };

	///! InfluxDB timestamp
	using Timestamp = long long;
	///! Tag set, used to specify the tags of a measurement. Pairs should be <tag_name, tag_value>.
	using TagSet = std::vector<std::pair<std::string, std::string>>;
	///! Field set, used to specify the fields in a measurement. Pairs should be <field_name, field_value>.
	using FieldSet = std::vector<std::pair<std::string, std::string>>;
	///! A single measurement/metric
	struct Metric {
		std::string measurement;	///< Measurement name.
		TagSet tags;				///< All associated tags.
		FieldSet fields;			///< All associated fields.
		Timestamp timestamp;		///< Timestamp, in nano-seconds (ms).
	};
	///! A set of measurements, used internally.
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
	bool write(const std::string& measurement, const TagSet& tags, const FieldSet& fields, long long timestamp = getCurrentTimestamp());

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

protected:
	Config *config;							///< InfluxDB configuration.
	LogTree &logtree;						///< Log target.

private:
	// Console commands
	friend class influxdb_status;
	friend class influxdb_config;

private:
	void _backgroundTask();
	bool push(const MetricSet& metrics);

	std::unique_ptr<MetricSet> collector = nullptr;
	SemaphoreHandle_t collectorMutex = NULL;

	TickType_t flushTicks = 0;
	SemaphoreHandle_t flushMutex = NULL;

#ifdef INFLUXDB_STATS
	unsigned int pushedMeasurements = 0;
	unsigned int droppedMeasurements = 0;
#endif
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_INFLUXDB_INFLUXDB_H_ */
