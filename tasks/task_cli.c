#include "user_tasks.h"

#include <stdio.h>
#include <string.h>

#include "cli.h"

// ----- TASK PROPERTIES -----
static TaskHandle_t sTH; // task handle
static uint8_t sPrio = 5; // priority
static uint16_t sStkSize = 2048; // stack size
void task_cli(void *pParam); // taszk routine function
// ---------------------------

// structure for defining cli commands
struct CliCommand
{
    CliToken_Type ppTok[MAX_TOKEN_N]; // tokens
    uint8_t tokCnt; // number of tokens in command without arguments
    uint8_t minArgCnt; // minimal number of parameters
    char pHelp[MAX_HELP_LEN]; // help line
    fnCliCallback pCB; // processing callback function
};

#define CLI_MAX_CMD_CNT (16) // limit on number of separate commands
static struct CliCommand spCliCmds[CLI_MAX_CMD_CNT];
static uint8_t sCliCmdCnt;

// ---------------------------


// register and initialize task
void reg_task_cli()
{
    BaseType_t result = xTaskCreate(task_cli, "cli", sStkSize, NULL, sPrio,
                                    &sTH);
    if (result != pdPASS)
    { // taszk létrehozása
        MSG("Failed to create task! (errcode: %d)\n", result);
    }

    // ----------------------
    sCliCmdCnt = 0;
}

// remove task fro
void unreg_task_cli()
{
    vTaskDelete(sTH); // taszk törlése
}

// ---------------------------

#define CLI_BUF_LENGTH (128)
#define TOK_ARR_LEN (16)

static void tokenize_cli_line(char *pLine, CliToken_Type ppTok[],
                              uint32_t tokMaxLen, uint32_t tokMaxCnt,
                              uint32_t *pTokCnt)
{
    uint32_t len = strlen(pLine);

    // copy to prevent modifying original one
    static char pLineCpy[CLI_BUF_LENGTH];
    strcpy(pLineCpy, pLine);

    *pTokCnt = 0;

    // prevent processing if input is empty
    if (len == 0 || tokMaxCnt == 0)
    {
        return;
    }

    // first token
    char *pTok = strtok(pLineCpy, " ");
    strncpy(&ppTok[0][0], pTok, tokMaxLen);
    (*pTokCnt)++;

    // further tokens
    while ((*pTokCnt < tokMaxCnt) && (pTok != NULL))
    {
        pTok = strtok(NULL, " ");

        if (pTok != NULL)
        {
            strncpy(&ppTok[*pTokCnt][0], pTok, tokMaxLen); // store token
            (*pTokCnt)++; // increment processed token count
        }
    }

}

static void process_cli_line(char *pLine)
{
    CliToken_Type ppTok[TOK_ARR_LEN];
    uint32_t tokCnt = 0;

    // tokenize line received from user input
    tokenize_cli_line(pLine, ppTok, MAX_TOK_LEN, TOK_ARR_LEN, &tokCnt);

    if (tokCnt == 0)
    {
        return;
    }

    int ret = -1;

    // print help
    if (!strcmp(ppTok[0], "?"))
    {
        MSG("\n\n? \t Print this help\n");

        uint8_t i;
        for (i = 0; i < sCliCmdCnt; i++) {
            MSG("%s\n", spCliCmds[i].pHelp);
        }

        MSG("\n\n");

        ret = 0;
    } else {
        // lookup command
        uint8_t i, k = 0, matchCnt = 0;
        int8_t n = -1;
        for (i = 0; i < sCliCmdCnt; i++) {
            matchCnt = 0;

            for (k = 0; k < spCliCmds[i].tokCnt && k < tokCnt; k++) {
                if (strcmp(ppTok[k], spCliCmds[i].ppTok[k])) {
                    break;
                } else {
                    matchCnt++;
                    if (matchCnt == spCliCmds[i].tokCnt) {
                        n = i;
                        break;
                    }
                }
            }

            if (n != -1) {
                break;
            }
        }

        // call command callback function
        if (n < 0) {
            ret = -1;
        } else {
            struct CliCommand * pCmd = &spCliCmds[n];
            uint8_t argc = tokCnt - pCmd->tokCnt;

            if (argc < pCmd->minArgCnt) {
                MSG("Insufficient parameters, see help! (?)\n", pLine);
            } else {
                ret = pCmd->pCB(&ppTok[pCmd->tokCnt], argc);
            }
        }
    }

    if (ret < 0)
    {
        MSG("Unknown command or bad parameter: '%s', see help! (?)\n", pLine);
    }
}

// task routine function
void task_cli(void *pParam)
{
    char pBuf[CLI_BUF_LENGTH + 1];

    while (1)
    {
        UARTgets(pBuf, CLI_BUF_LENGTH);
        process_cli_line(pBuf);
    }
}

static void cli_remove_command(int cmdIdx) {
    if (cmdIdx + 1 > sCliCmdCnt || cmdIdx < 0) {
        return;
    }

    uint8_t i;
    for (i = cmdIdx; i < sCliCmdCnt - 1; i++) {
        memcpy(&spCliCmds[i], &spCliCmds[i + 1], sizeof(struct CliCommand));
    }

    sCliCmdCnt--;
}

void cli_register_command(char *pCmdParsHelp, uint8_t cmdTokCnt, uint8_t minArgCnt, fnCliCallback pCB) {
    struct CliCommand *pCmd = &spCliCmds[sCliCmdCnt];

    // tokenize the first part of the line (run until cmkTokCnt tokens have been fetched)
    uint32_t tokCnt = 0;
    tokenize_cli_line(pCmdParsHelp, pCmd->ppTok, MAX_TOK_LEN, (cmdTokCnt > TOK_ARR_LEN ? TOK_ARR_LEN : cmdTokCnt), &tokCnt);
    pCmd->tokCnt = (uint8_t) tokCnt;

    // store minimal argument count parameter
    pCmd->minArgCnt = minArgCnt;

    // copy help line
    strncpy(pCmd->pHelp, pCmdParsHelp, MAX_HELP_LEN);

    // store callback function pointer
    pCmd->pCB = pCB;

    // increase the amount of commands stored
    sCliCmdCnt++;

    // clean up if the same command has been registered before
    uint8_t i, t;
    int duplicate_idx = -1;
    for (i = 0; i < (sCliCmdCnt - 1); i++) {
        if (spCliCmds[i].tokCnt == cmdTokCnt) {
            for (t = 0; t < cmdTokCnt; t++) {
                if (strcmp(spCliCmds[i].ppTok[t], pCmd->ppTok[t])) {
                    break;
                }
            }

            if (t == cmdTokCnt) {
                duplicate_idx = i;
                break;
            }
        }
    }

    if (duplicate_idx > -1) {
        cli_remove_command(duplicate_idx);
    }
}

