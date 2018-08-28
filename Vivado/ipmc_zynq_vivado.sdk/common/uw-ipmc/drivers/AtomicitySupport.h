/*
 * AddressableChainSupport.h
 *
 *  Created on: Aug 20, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_ATOMICITYSUPPORT_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_ATOMICITYSUPPORT_H_

#include <FreeRTOS.h>
#include <libs/ThreadingPrimitives.h>
#include <semphr.h>
#include <functional>

// TODO: Consider adding a throw when a lock timeout takes place
// Maybe add a set of default throws in ThreadingPrimitives.h?

/**
 * Adds support for atomicity in drivers.
 * If the driver supports select then use AddressableAtomicitySupport instead.
 * Base class provides a mutex that should be used in critical functions and
 * also the atomic function which allows chaining of operations.
 */
class AtomicitySupport {
public:
	AtomicitySupport() {
		this->mutex = xSemaphoreCreateMutex();
		configASSERT(this->mutex);
	};

	~AtomicitySupport() {
		vSemaphoreDelete(this->mutex);
	};

	/**
	 * Chain a set of operations. The set of operations is thread-safe as a whole.
	 * @param lambda_func The function/lambda that holds the sequence of operations.
	 * @return The return value of the function/lambda is returned.
	 */
	template<typename T> T atomic(std::function<T()> lambda_func) {
		MutexLock(this->mutex);
		T r = lambda_func();
		return r;
	}

	///! Same as AtomicitySupport::atomic, but where no return is required.
	void atomic(std::function<void()> lambda_func) {
		MutexLock(this->mutex);
		lambda_func();
	}

protected:
	SemaphoreHandle_t mutex;	///< Mutex to also be used on the driver. MutexLock is recommended.
};

/**
 * Same as AtomicitySupport while also supporting addressable devices.
 * Driver needs to implement select and deselect.
 * atomic operation now selects and deselects the target device automatically.
 */
class AddressableAtomicitySupport : public AtomicitySupport {
public:
	/**
	 * Select the target device.
	 * @param address Address of the target device.
	 */
	virtual void select(uint32_t address) = 0;

	///! De-select the addressed device.
	virtual void deselect() = 0;

	/**
	 * Chain a set of operations while a device is kept select.
	 * The chain of operations is thread-safe as a whole.
	 * @param address Address of the target device.
	 * @param lambda_func The function/lambda that holds the sequence of operations.
	 * @param dont_deselect Set to false if the device should be left selected afterwards.
	 * @return The return value of the function/lambda is returned.
	 */
	template<typename T> T atomic(uint32_t address, std::function<T()> lambda_func, bool dont_deselect = true) {
		MutexLock(this->mutex);
		this->select(address);
		T r = lambda_func();
		if (dont_deselect) this->deselect();
		return r;
	}

	///! Same as AddressableAtomicitySupport::atomic, but where no return is required.
	void atomic(uint32_t address, std::function<void()> lambda_func, bool dont_deselect = true) {
		MutexLock(this->mutex);
		this->select(address);
		lambda_func();
		if (dont_deselect) this->deselect();
	}
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_ATOMICITYSUPPORT_H_ */
