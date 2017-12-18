#include <stdio.h>
#include "limits.h"
#include "string.h"

#include <FreeRTOS.h>
#include <task.h>
#include <IPMC.h>
#include <drivers/ps_uart/PSUART.h>

/* Xilinx includes. */
#include "xparameters.h"
#include "xscutimer.h"
#include "xscugic.h"
#include "xil_exception.h"

extern "C" {
#include <lwip/opt.h>
#include "ethernet/ethernet.h"
}

extern "C" {
/* The private watchdog is used as the timer that generates run time stats.  */
XScuWdt xWatchDogInstance;

/*-----------------------------------------------------------*/

void vAssertCalled(const char * pcFile, unsigned long ulLine) {
	volatile unsigned long ul = 0;

	(void) pcFile;
	(void) ulLine;

	taskENTER_CRITICAL()
	;
	{
		/* Set ul to a non-zero value using the debugger to step out of this
		 function. */
		while (ul == 0) {
			/* Let's trigger the debugger directly, if attached.
			 *
			 * No sense asserting and not realizing it.
			 *
			 * Step over this instruction to continue.
			 */
			__asm__("bkpt");

			portNOP();
		}
	}
	taskEXIT_CRITICAL()
	;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
	/* If the buffers to be provided to the Idle task are declared inside this
	 function then they must be declared static - otherwise they will be allocated on
	 the stack and so not exists after this function exits. */
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	 state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	 Note that, as the array is necessarily of type StackType_t,
	 configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
 application must provide an implementation of vApplicationGetTimerTaskMemory()
 to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
	/* If the buffers to be provided to the Timer task are declared inside this
	 function then they must be declared static - otherwise they will be allocated on
	 the stack and so not exists after this function exits. */
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	 task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	 Note that, as the array is necessarily of type StackType_t,
	 configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*-----------------------------------------------------------*/

void vInitialiseTimerForRunTimeStats(void) {
	XScuWdt_Config *pxWatchDogInstance;
	uint32_t ulValue;
	const uint32_t ulMaxDivisor = 0xff, ulDivisorShift = 0x08;

	pxWatchDogInstance = XScuWdt_LookupConfig( XPAR_SCUWDT_0_DEVICE_ID);
	XScuWdt_CfgInitialize(&xWatchDogInstance, pxWatchDogInstance, pxWatchDogInstance->BaseAddr);

	ulValue = XScuWdt_GetControlReg(&xWatchDogInstance);
	ulValue |= ulMaxDivisor << ulDivisorShift;
	XScuWdt_SetControlReg(&xWatchDogInstance, ulValue);

	XScuWdt_LoadWdt(&xWatchDogInstance, UINT_MAX);
	XScuWdt_SetTimerMode(&xWatchDogInstance);
	XScuWdt_Start(&xWatchDogInstance);
}

/*-----------------------------------------------------------*/

static void prvSetupHardware(void) {
	BaseType_t xStatus;
	XScuGic_Config *pxGICConfig;

	/* Ensure no interrupts execute while the scheduler is in an inconsistent
	 state.  Interrupts are automatically enabled when the scheduler is
	 started. */
	portDISABLE_INTERRUPTS();

	/* Obtain the configuration of the GIC. */
	pxGICConfig = XScuGic_LookupConfig( XPAR_SCUGIC_SINGLE_DEVICE_ID);

	/* Sanity check the FreeRTOSConfig.h settings are correct for the
	 hardware. */
	configASSERT(pxGICConfig);
	configASSERT(pxGICConfig->CpuBaseAddress == ( configINTERRUPT_CONTROLLER_BASE_ADDRESS + configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET ));
	configASSERT(pxGICConfig->DistBaseAddress == configINTERRUPT_CONTROLLER_BASE_ADDRESS);

	/* Install a default handler for each GIC interrupt. */
	xStatus = XScuGic_CfgInitialize(&xInterruptController, pxGICConfig, pxGICConfig->CpuBaseAddress);
	configASSERT(xStatus == XST_SUCCESS);
	(void) xStatus; /* Remove compiler warning if configASSERT() is not defined. */

	vPortInstallFreeRTOSVectorTable();
}
} // extern "C"

//#define PS_UART_INITIAL_TEST
static void TaskPrinter(void *dummy0) {
	printf("TaskPrinter Started\n");
	LogTree *tasklog = &LOG["task_listing"];
	char buf[518];
	while (1) {
		char *wbuf = buf;
#ifdef __cplusplus
		printf("C++\n");
#else
		printf("C--\n");
#endif
		//u32 int_mask = XUartPs_GetInterruptMask(&uart0.UartInstPtr);
		//printf("test 0x%08x \n", int_mask);
		vTaskDelay(10000);
		vPortEnterCritical();
		*(wbuf++) = '\n';
		vTaskList(wbuf);
		wbuf += strlen(wbuf);
		*(wbuf++) = '\n';
		vTaskGetRunTimeStats(wbuf);
		wbuf += strlen(wbuf);
		*(wbuf++) = '\n';
		*(wbuf++) = '\0';
		vPortExitCritical();
		tasklog->log(buf, LogTree::LOG_DIAGNOSTIC);
		uart_ps0->write(buf, strlen(buf), portMAX_DELAY);
	}
}

const char *banner =
	"********************************************************************************\n"
	"\n"
	"University of Wisconsin IPMC " GIT_DESCRIBE "\n"
#ifdef GIT_STATUS
	"\n"
	GIT_STATUS // contains a trailing \n
#endif
	"\n"
	"********************************************************************************\n"
	;

int main() {
	/*
	 * If you want to run this application outside of SDK,
	 * uncomment one of the following two lines and also #include "ps7_init.h"
	 * or #include "ps7_init.h" at the top, depending on the target.
	 * Make sure that the ps7/psu_init.c and ps7/psu_init.h files are included
	 * along with this example source files for compilation.
	 */
	/* ps7_init();*/
	/* psu_init();*/

	/* See http://www.freertos.org/RTOS-Xilinx-Zynq.html. */
	prvSetupHardware();

	driver_init(true);
	ipmc_service_init();

	std::string bannerstr(banner);
	windows_newline(bannerstr);
	uart_ps0->write(bannerstr.data(), bannerstr.size(), 0);

	xTaskCreate(lwip_startup_thread, "lwip_start", configMINIMAL_STACK_SIZE, NULL, configLWIP_TASK_PRIORITY, NULL);
    xTaskCreate(TaskPrinter, "TaskPrint", configMINIMAL_STACK_SIZE+256, NULL, configMAX_PRIORITIES, NULL);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	 line will never be reached.  If the following line does execute, then
	 there was either insufficient FreeRTOS heap memory available for the idle
	 and/or timer tasks to be created, or vTaskStartScheduler() was called from
	 User mode.  See the memory management section on the FreeRTOS web site for
	 more details on the FreeRTOS heap http://www.freertos.org/a00111.html.  The
	 mode from which main() is called is set in the C start up code and must be
	 a privileged mode (not user mode). */
	for (;;)
		;

	return 0;
}
