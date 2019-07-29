/*
 * SensorSource.h
 *
 *  Created on: Aug 14, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_SENSORSOURCE_H_
#define SRC_COMMON_UW_IPMC_LIBS_SENSORSOURCE_H_

// TODO: Not being used, can be removed later on

#include <stdint.h>
#include <vector>
#include <string>

///! Allows the bit conversion of all data types
#define SENSORDATA(x) (*((uint32_t*)&(x)))

class SensorSource {
public:
	//SensorSource() {};
	//virtual ~SensorSource() {};

	typedef enum {
		UNIT_CELSIUS,
		UNIT_VOLT,
		UNIT_AMPERE
		// TODO: Extend
	} SensorUnit;

	static std::string sensorUnitToString(const SensorUnit& u);

	typedef struct {
		const char* source; ///< Source name.
		const char* sensor; ///< Sensor name.
		SensorUnit unit;
		union {
			uint32_t data;
			float _f;
			uint32_t _u;
			int32_t _i;
		};
	} SensorItem;

	typedef std::vector<SensorItem> SensorList;

protected:
	virtual SensorList refreshSensorList() = 0;
	virtual void startRefreshTask(const std::string& taskname, const uint32_t ms_interval = 1000) final;
	virtual void changeRefreshInterval(const uint32_t ms_interval) final;

private:
	void _backgroundTask();

	std::string taskName;
	uint32_t tickInterval;
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_SENSORSOURCE_H_ */
