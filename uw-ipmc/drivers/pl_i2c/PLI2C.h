/*
 * PLI2C.h
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_I2C_PLI2C_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_I2C_PLI2C_H_

#include <xiic.h>
#include "drivers/generics/I2C.h"
#include "semphr.h"

/**
 * A FreeRTOS interrupt based driver for Xilinx IIC IP core.
 */
class PL_I2C : public I2C {
public:
	PL_I2C(u16 DeviceId, u32 IntrId);
	virtual ~PL_I2C();

	size_t read(u8 addr, u8 *buf, size_t len, TickType_t timeout=portMAX_DELAY);
	size_t write(u8 addr, const u8 *buf, size_t len, TickType_t timeout=portMAX_DELAY);

	void _HandleRecv(int ByteCount);  ///< \protected Internal.
	void _HandleSend(int ByteCount);  ///< \protected Internal.
	void _HandleStatus(int Status);   ///< \protected Internal.
private:
	u16 DeviceId;
	u32 IntrId;

	SemaphoreHandle_t Mutex;
	QueueHandle_t IRQ_q;

	volatile struct {
		u32 EventStatus;
		u32 RemainingRecvBytes;
		u32 RemainingSendBytes;
		bool EventStatusUpdated;
		bool RecvBytesUpdated;
		bool SendBytesUpdated;
	} HandlerInfo;

	XIic IicInst;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_I2C_PLI2C_H_ */
