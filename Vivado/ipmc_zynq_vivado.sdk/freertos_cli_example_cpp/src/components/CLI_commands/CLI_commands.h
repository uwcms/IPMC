/*
 * CLI_commands.h
 *
 *  Created on: Aug 31, 2017
 *      Author: mpvl
 */

#ifndef _CLI_COMMANDS_H_
#define _CLI_COMMANDS_H_

#include <FreeRTOS.h>

#ifndef  configINCLUDE_TRACE_RELATED_CLI_COMMANDS
	#define configINCLUDE_TRACE_RELATED_CLI_COMMANDS 0
#endif

#ifndef configINCLUDE_QUERY_HEAP_COMMAND
	#define configINCLUDE_QUERY_HEAP_COMMAND 0
#endif

/*
 * The function that registers the commands that are defined within this file.
 */
void vRegisterCLICommands( void );

#endif /* _CLI_COMMANDS_H_ */
