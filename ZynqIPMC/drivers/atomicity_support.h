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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_ATOMICITY_SUPPORT_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_ATOMICITY_SUPPORT_H_

#include <FreeRTOS.h>
#include <libs/ThreadingPrimitives.h>
#include <semphr.h>
#include <functional>

// TODO: Consider adding a throw when a lock timeout takes place
// Maybe add a set of default throws in ThreadingPrimitives.h?

/**
 * Adds support for atomicity in drivers, exposing a mutex that can be used in
 * critical sections of the driver itself and also a function that allows to
 * chain several non-thread safe commands safely.
 *
 * If the driver supports multiple interfaces or devices that can be addressed
 * within the driver or in multiple parallel tasks then use AddressableAtomicitySupport
 * instead.
 *
 * Base class provides a mutex that should be used in critical functions and
 * also the atomic function which allows chaining of operations.
 */
class AtomicitySupport {
public:
	AtomicitySupport() {
		this->mutex = xSemaphoreCreateMutex();
		configASSERT(this->mutex);
	};

	virtual ~AtomicitySupport() {
		vSemaphoreDelete(this->mutex);
	};

	/**
	 * Chain a set of operations. The set of operations is thread-safe as a group.
	 * @param lambda_func Function/lambda that holds the sequence of operations.
	 * @return The return value of the function/lambda is returned.
	 */
	template<typename T> T atomic(std::function<T()> lambda_func) {
		MutexGuard<false> lock(this->mutex, true);
		T r = lambda_func();
		return r;
	}

	//! Same as AtomicitySupport::atomic, but where no return is required.
	void atomic(std::function<void()> lambda_func) {
		MutexGuard<false> lock(this->mutex, true);
		lambda_func();
	}

	//! Useful to check if we are in atomic operation. Functions that require it can throw an error.
	inline bool inAtomic() const { return !uxSemaphoreGetCount(this->mutex); };

protected:
	SemaphoreHandle_t mutex;	///< Mutex to also be used on the driver. Use of MutexGuard is recommended.
};

/**
 * Same as AtomicitySupport while also supporting addressable devices.
 * Driver needs to implement select and deselect.
 * atomic operation selects and deselects the target device automatically.
 */
class AddressableAtomicitySupport : public AtomicitySupport {
public:
	/**
	 * Select the target device.
	 * @param address Address of the target device.
	 */
	virtual void select(size_t address) = 0;

	//! De-select the addressed device.
	virtual void deselect() = 0;

	/**
	 * Chain a set of operations while a device is kept select.
	 * The chain of operations is thread-safe and similar accesses won't be allowed
	 * until all operations in the chain are executed.
	 * @param address Address of the target device.
	 * @param lambda_func Function/lambda that holds the sequence of operations.
	 * @param dont_deselect Set to false if the device should be left selected afterwards.
	 * @return The return value of the function/lambda is returned.
	 */
	template<typename T> T atomic(uint32_t address, std::function<T()> lambda_func, bool dont_deselect = true) {
		MutexGuard<false> lock(this->mutex, true);
		this->select(address);
		T r = lambda_func();
		if (dont_deselect) this->deselect();
		return r;
	}

	//! Same as AddressableAtomicitySupport::atomic, but where no return is required.
	void atomic(uint32_t address, std::function<void()> lambda_func, bool dont_deselect = true) {
		MutexGuard<false> lock(this->mutex, true);
		this->select(address);
		lambda_func();
		if (dont_deselect) this->deselect();
	}
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_ATOMICITY_SUPPORT_H_ */
