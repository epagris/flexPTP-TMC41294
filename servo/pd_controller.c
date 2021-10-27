/* (C) András Wiesner, 2021 */

#include "pd_controller.h"

#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "utils.h"

// ----------------------------------

static float P_FACTOR = 0.5 * 0.476;
static float D_FACTOR = 2.0 * 0.476;

// ----------------------------------

static int32_t dt_prev; // clock difference measured in previous iteration (needed for differentiation)

// ----------------------------------

static int CB_params(const CliToken_Type *ppArgs, uint8_t argc)
{
    // set if parameters passed after command
    if (argc >= 2)
    {
        P_FACTOR = atof(ppArgs[0]);
        D_FACTOR = atof(ppArgs[1]);
    }

    char pL[32];
    sprintf(pL, "K_p = %.3f, K_d = %.3f", P_FACTOR, D_FACTOR);
    MSG("> PTP params: %s\n", pL);

    return 0;
}

static void pd_ctrl_register_cli_commands() {
    cli_register_command("ptp servo params [Kp Kd] \t\t\tSet or query K_p and K_d servo parameters", 3, 0, CB_params);
}

void pd_ctrl_init() {
    pd_ctrl_reset();
    pd_ctrl_register_cli_commands();
}

void pd_ctrl_reset() {
    dt_prev = 0;
}

float pd_ctrl_run(int32_t dt) {
    if (dt_prev == 0) {
        dt_prev = dt;
        return 0;
    }

    // calculate difference
    int32_t d_D = dt - dt_prev;

    // calculate output (run the PD controller)
    float corr_ppb = -(dt * P_FACTOR + d_D * D_FACTOR);

    // store error value (time difference) for use in next iteration
    dt_prev = dt;

    return corr_ppb;
}

// ----------------------------------
