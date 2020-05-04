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

#include <services/console/consolesvc.h>
#include <libs/threading.h>
#include <libs/printf.h>
#include <services/faultlog/faultlog.h>
#include <core.h>
#include <stdint.h>
#include <sys/time.h>

#define FAULT_SEQUENCE_INVALID 0xFF
#define FAULT_SEQUENCE_INITIAL 0

static inline uint8_t incr_seq(uint8_t sequence) {
	return (sequence + 1) % 255;
}
static inline uint8_t decr_seq(uint8_t sequence) {
	if (sequence == 0)
		return 254;
	else
		return sequence-1;
}

FaultLog::FaultLog(PersistentStorage &persistent_storage, LogTree &logtree)
	: persistent_storage(persistent_storage), logtree(logtree) {
	this->mutex = xSemaphoreCreateMutex();

	bool initialize = false;
	if (this->persistent_storage.getSectionVersion(PersistentStorageAllocations::WISC_FAULT_LOG) != 2) {
		initialize = true;
		this->persistent_storage.deleteSection(PersistentStorageAllocations::WISC_FAULT_LOG);
	}
	this->faultlog = (struct faultlog*)this->persistent_storage.getSection(PersistentStorageAllocations::WISC_FAULT_LOG, 2, sizeof(struct FaultLog::faultlog));
	if (!this->faultlog || this->faultlog->max_record_count != this->fault_log_max_records) {
		this->persistent_storage.deleteSection(PersistentStorageAllocations::WISC_FAULT_LOG);
		this->faultlog = (struct faultlog*)this->persistent_storage.getSection(PersistentStorageAllocations::WISC_FAULT_LOG, 2, sizeof(struct FaultLog::faultlog));
	}
	if (!this->faultlog)
		throw std::runtime_error("Unable to allocate or initialize fault log storage.");

	if (initialize) {
		this->faultlog->max_record_count = this->fault_log_max_records;
		this->faultlog->verbosity_config = 0xffffffff; // "uninitialized"
		memset(this->faultlog->log, FAULT_SEQUENCE_INVALID, sizeof(struct FaultLog::fault)*this->fault_log_max_records);
		this->persistent_storage.flush();
	}

	// Now we have to scan the table and deduce the current read position.
	bool record_found = false;
	for (size_t i = 0; i < this->fault_log_max_records; ++i) {
		if (this->faultlog->log[i].sequence == FAULT_SEQUENCE_INVALID) {
			// An uninitialized record was found.
			this->next_record = i;
			this->next_sequence = FAULT_SEQUENCE_INITIAL;
			record_found = true;
			break;
		}
		else if (this->faultlog->log[(i+1) % this->fault_log_max_records].sequence != incr_seq(this->faultlog->log[i].sequence)) {
			// The next record doesn't follow this one.  This is therefore the last.
			this->next_record = (i+1) % this->fault_log_max_records;
			this->next_sequence = incr_seq(this->faultlog->log[i].sequence);
			record_found = true;
			break;
		}
	}
	if (!record_found) {
		this->logtree.log("Unable to locate next writable FaultLog record! The FaultLog is CORRUPT.", LogTree::LOG_CRITICAL);
		this->next_record = 0;
		this->next_sequence = FAULT_SEQUENCE_INITIAL;
	}
}

FaultLog::~FaultLog() {
	this->persistent_storage.flush();
	vSemaphoreDelete(this->mutex);
}

void FaultLog::submit(struct fault &fault) {
	fault.sequence = this->next_sequence;
	timeval tv;
	gettimeofday(&tv, nullptr);
	fault.unixtime = tv.tv_sec;
	MutexGuard<false> lock(this->mutex);
	memcpy(&this->faultlog->log[this->next_record], &fault, sizeof(fault));
	this->persistent_storage.flush(&this->faultlog->log[this->next_record], 16);
	this->next_record = (this->next_record + 1) % this->fault_log_max_records;
	this->next_sequence = incr_seq(this->next_sequence);
}

std::deque<struct FaultLog::fault> FaultLog::dump() const {
	MutexGuard<false> lock(this->mutex);
	std::deque<struct FaultLog::fault> faults;

	if (this->next_record == 0 && this->faultlog->log[this->next_record].sequence == FAULT_SEQUENCE_INVALID) {
		// We have a blank fault log.  It's never had a record written to it.
		return faults;
	}

	// Next up, decrement until we hit a sequence number discontinuity, and push faults onto the list.
	int pos = this->next_record - 1;
	if (pos < 0)
		pos = this->fault_log_max_records - 1;
	struct fault fault;
	do {
		// This record is known to exist.  We bailed on an empty log above, and
		// will stop iteration accordingly below.
		memcpy(&fault, &this->faultlog->log[pos], sizeof(struct fault));
		faults.push_front(fault);
		if (--pos < 0)
			pos = this->fault_log_max_records - 1;
	} while (faults.size() < this->fault_log_max_records /* final safety check for corrupt logs */
			&& this->faultlog->log[pos].sequence != FAULT_SEQUENCE_INVALID /* not free space */
			&& this->faultlog->log[pos].sequence == decr_seq(fault.sequence) /* not sequence discontinuity */);
	return faults;
}

void FaultLog::erase() {
	MutexGuard<false> lock(this->mutex);
	memset(this->faultlog->log, FAULT_SEQUENCE_INVALID, sizeof(struct FaultLog::fault)*this->fault_log_max_records);
	this->persistent_storage.flush();
	this->next_record = 0;
	this->next_sequence = FAULT_SEQUENCE_INITIAL;
}


/// A "dump" console command.
class FaultLog::DumpCommand : public CommandParser::Command {
public:
	DumpCommand(FaultLog &faultlog) : faultlog(faultlog) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Dump the contents of the fault log to the console.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::deque<struct FaultLog::fault> faults = this->faultlog.dump();
		console->write(stdsprintf("%-20s %s\n", "Time", "Fault Information"));
		for (const struct fault fault : faults) {
		    std::string formatted;

		    switch (fault.fault_type) {
			case FaultLog::fault::FAULT_TYPE_MZ_FAULTED:
			{
				std::string mzname = "Unnamed";
				if (payload_manager)
					mzname = payload_manager->getMZName(fault.mz_fault.mz_number);
				if (mzname.size() == 0)
					mzname = "Unnamed";
				formatted = stdsprintf("MZ %hhu (%s) faulted.", fault.mz_fault.mz_number, mzname.c_str());
				break;
			}
			case FaultLog::fault::FAULT_TYPE_SENSOR_EVENT:
			{
				formatted += stdsprintf("IPMI:%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
						fault.sensor_event.ipmi_eventmsg[0],
						fault.sensor_event.ipmi_eventmsg[1],
						fault.sensor_event.ipmi_eventmsg[2],
						fault.sensor_event.ipmi_eventmsg[3],
						fault.sensor_event.ipmi_eventmsg[4],
						fault.sensor_event.ipmi_eventmsg[5],
						fault.sensor_event.ipmi_eventmsg[6]
						);

				std::shared_ptr<Sensor> sensor = ipmc_sensors.get(fault.sensor_event.ipmi_eventmsg[2]);
				if (sensor) {
					std::shared_ptr<const SensorDataRecordReadableSensor> tsdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->getSdrKey()));
					if (tsdr) {
						static const std::string threshold_names[12] = {
								"lnc-",
								"lnc+",
								"lcr-",
								"lcr+",
								"lnr-",
								"lnr+",
								"unc-",
								"unc+",
								"ucr-",
								"ucr+",
								"unr-",
								"unr+"
						};
						std::string name = tsdr->id_string().c_str();
						std::string threshold;
						if ((fault.sensor_event.ipmi_eventmsg[4] & 0x0f) < 12)
							threshold = threshold_names[fault.sensor_event.ipmi_eventmsg[4] & 0x0f];
						else
							threshold = stdsprintf("[invalid threshold: %hhu]", fault.sensor_event.ipmi_eventmsg[4] & 0x0f);
						float value = tsdr->toFloat(fault.sensor_event.ipmi_eventmsg[5]);
						float threshold_value = tsdr->toFloat(fault.sensor_event.ipmi_eventmsg[6]);
						formatted += stdsprintf(" Sensor %2hhu %-15s val %6.3f is beyond %s %6.3f.",
								// "%s " -> "\n                    ",
								fault.sensor_event.ipmi_eventmsg[2],
								(std::string("(")+name+")").c_str(),
								(double)value,
								threshold.c_str(),
								(double)threshold_value
								);
					}
				}
				break;
			}
			case FaultLog::fault::FAULT_OEM:
			{
				formatted = "OEM:";
				for (int i = 0; i < 10; ++i)
					formatted += stdsprintf(" 0x%02hhx", fault.fault_data[i]);
				break;
			}
			default:
			{
				formatted = stdsprintf("UNKNOWN[%hhu]:", (uint8_t)fault.fault_type);
				for (int i = 0; i < 10; ++i)
					formatted += stdsprintf(" 0x%02hhx", fault.fault_data[i]);
				break;
			}
			}

			time_t ts = fault.unixtime;
		    struct tm* tm_info = localtime(&ts);
		    char timestring[21];
		    strftime(timestring, 21, "%Y-%m-%dT%H:%M:%SZ", tm_info);
		    timestring[20] = '\0'; // safety

		    console->write(stdsprintf("%-20s %s\n", timestring, formatted.c_str()));
		}
		console->write(stdsprintf("Found %d entries (capacity %d).\n", faults.size(), FaultLog::fault_log_max_records));
	}

private:
	FaultLog &faultlog;
};

/// An "erase" console command.
class FaultLog::EraseCommand : public CommandParser::Command {
public:
	EraseCommand(FaultLog &faultlog) : faultlog(faultlog), erasekey(0) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " $erase_key\n\n"
				"Erase the fault log.\n"
				"\n"
				"An erase key will be generated whenever the command is run.\n"
				"You must run it again with the latest key to confirm the erase.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::deque<struct FaultLog::fault> faults = this->faultlog.dump();
		uint16_t currentkey = this->erasekey, inputkey = 0;
		parameters.parseParameters(1, true, &inputkey);
		this->erasekey = (get_tick64() & 0xffff)|1; // Not a great random function but good enough for this.
		if (currentkey != inputkey || currentkey == 0) {
			console->write(stdsprintf("Erase Key: %hu\n", this->erasekey));
		}
		else {
			console->write("Erasing...\n");
			faultlog.erase();
		}
	}

private:
	FaultLog &faultlog;
	uint16_t erasekey;
};

namespace {
/// An "erase" console command.
class VerbosityCommand : public CommandParser::Command {
public:
	VerbosityCommand(FaultLog &faultlog) : faultlog(faultlog) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [$new_verbosity]\n\n"
				"Configure the fault log verbosity mask.\n"
				"\n"
				"The fault log verbosity mask determines what events are considered significant\n"
				"enough to log.  In the default design, this is a bitmask of IPMI sensor\n"
				"thresholds and applies only to threshold sensors.\n"
				"\n"
				"These bits, in ascending order, are:"
				"  lnc- lnc+ lcr- lcr+ lnr- lnr+ unc- unc+ ucr- ucr+ unr- unr+\n"
				"\n"
				"Useful example values include:\n"
				"  0x810 (unr+ lnr-)\n"
				"  0xA14 (unr+ ucr+ lnr- lcr-)\n"
				"  0xA95 (unr+ ucr+ unc+ lnr- lcr- lnc-)\n"
				"\n"
				"Without a parameter, the current mask is shown.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint32_t newmask;
		if (parameters.parseParameters(1, true, &newmask)) {
			this->faultlog.set_verbosity_config(newmask);
		}
		else {
			console->write(stdsprintf("Verbosity mask: 0x%08lx\n", this->faultlog.get_verbosity_config()));
		}
	}

private:
	FaultLog &faultlog;
	uint16_t erasekey;
};
} // anonymous namespace

void FaultLog::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "dump", std::make_shared<FaultLog::DumpCommand>(*this));
	parser.registerCommand(prefix + "erase", std::make_shared<FaultLog::EraseCommand>(*this));
	parser.registerCommand(prefix + "verbosity", std::make_shared<VerbosityCommand>(*this));
}
void FaultLog::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "dump", nullptr);
	parser.registerCommand(prefix + "erase", nullptr);
	parser.registerCommand(prefix + "verbosity", nullptr);
}
