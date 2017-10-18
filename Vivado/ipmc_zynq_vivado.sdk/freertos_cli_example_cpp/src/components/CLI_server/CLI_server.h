/*
 * CLI_server.h
 *
 *  Created on: Aug 31, 2017
 *      Author: mpvl
 */

#ifndef _CLI_SERVER_H_
#define _CLI_SERVER_H_

/* Dimensions the buffer into which input characters are placed. */
#define cmdMAX_INPUT_SIZE	100

/* Dimensions the buffer into which string outputs can be placed. */
#define cmdMAX_OUTPUT_SIZE	1024

void CLI_Interpreter_Task(void *pvParameters);

#endif /* _CLI_SERVER_H_ */
