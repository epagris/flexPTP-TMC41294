/*
 * cli.h
 *
 *  Created on: 2021. szept. 16.
 *      Author: epagris
 */

#ifndef TASKS_CLI_H_
#define TASKS_CLI_H_

#define MAX_TOK_LEN (16) // maximal token length

typedef char CliToken_Type[MAX_TOK_LEN];

#define MAX_TOKEN_N (8) // maximal token count for a single command
#define MAX_HELP_LEN (96) // maximal help line length for a single command

typedef int (*fnCliCallback)(const CliToken_Type * ppArgs, uint8_t argc); // function prototype for a callbacks


void cli_register_command(char *pCmdParsHelp, uint8_t cmdTokCnt, uint8_t minArgCnt, fnCliCallback pCB); // register a new command

#endif /* TASKS_CLI_H_ */
