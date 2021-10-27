/* (C) András Wiesner, 2020 */

#ifndef PTP_H
#define PTP_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "utils/lwiplib.h"
#include "utils/uartstdio.h"

#include "driverlib/emac.h"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"
#include "driverlib/pin_map.h"

#include "timeutils.h"

// IP address of PTP-IGMP groups
#define PTP_IGMP_DEFAULT ("224.0.1.129")
#define PTP_IGMP_PEER_DELAY ("224.0.0.107")

// PTP UDP ports
#define PTP_PORT0 (319)
#define PTP_PORT1 (320)

#define PTP_DELAY_REQ_PCKT_SIZE (44)

// DEBUG switch for printing state transisitions
#define PRINT_STATE_TRANSITION_MESSAGES (0)

// PTP packet types
enum PTPMessageID
{
    PTPIDSync = 0,
    PTPIDDelay_Req = 1,
    PTPIDFollow_Up = 8,
    PTPIDDelay_Resp = 9
};

// PTP header control field values
enum PTPControl {
    PTPCONSync = 0,
    PTPCONDelay_Req = 1,
    PTPCONFollow_Up = 2,
    PTPCONelay_Resp = 3
};

// PTP flags structure
struct PTPFlags {
    bool PTP_SECURITY;
    bool PTP_ProfileSpecific_2;
    bool PTP_ProfileSpecific_1;
    bool PTP_UNICAST;
    bool PTP_TWO_STEP;
    bool PTP_ALTERNATE_MASTER;
    bool FREQUENCY_TRACEABLE;
    bool TIME_TRACEABLE;
    bool PTP_TIMESCALE;
    bool PTP_UTC_REASONABLE;
    bool PTP_LI_59;
    bool PTP_LI_61;
};

// PTP message header structure
struct PTPHeader
{
    // 0.
    uint8_t messageID; // ID
    uint8_t transportSpecific;

    // 1.
    uint8_t versionPTP; // PTP version
    uint8_t _r1;

    // 2-3.
    uint16_t messageLength; // length

    // 4.
    uint8_t subdomainNumber;

    // 5.
    uint8_t _r2;

    // 6-7.
    struct PTPFlags flags;

    // 8-15.
    uint64_t correction_ns; 
    uint32_t correction_subns; 

    // 16-19.
    uint32_t _r3;

    // 20-27.
    uint64_t clockIdentity;

    // 28-29
    uint16_t sourcePortID;

    // 30-31.
    uint16_t sequenceID;

    // 32.
    uint8_t control;

    // 33.
    uint8_t logMessagePeriod; // ...
};

// identification carrying Delay_Resp message
struct Delay_RespIdentification {
    uint64_t requestingSourceClockIdentity;
    uint16_t requestingSourcePortIdentity;
};

// Data to perform a full synchronization
struct SyncCycleData {
    struct TimestampI t1; // Sync transmission time by master clock
    struct TimestampI t2; // Sync reception time by slave clock
    struct TimestampI t3; // Delay_Req transmission time by slave clock
    struct TimestampI t4; // Delay_Resp reception time by master clock
};

// -------------------------------------------
// --- DEFINES FOR PORTING IMPLEMENTATION ----
// -------------------------------------------
//
// Include hardware port files and fill the defines below to port the PTP stack to a physical hardware:
// - PTP_HW_INIT(increment, addend): function initializing timestamping hardware
// - PTP_MAIN_OSCILLATOR_FREQ_HZ: clock frequency fed into the timestamp unit [Hz]
// - PTP_INCREMENT_NSEC: hardware clock increment [ns]
// - PTP_UPDATE_CLOCK(s,ns): function jumping clock by defined value (negative time value means jumping backward)
// - PTP_SET_ADDEND(addend): function writing hardware clock addend register
//
// Include the clock servo (controller) and define the following:
// - PTP_SERVO_INIT(): function initializing clock servo
// - PTP_SERVO_RESET(): function reseting clock servo
// - PTP_SERVO_RUN(d): function running the servo, input: master-slave time difference (error), return: clock tuning value in PPB
//
// -------------------------------------------

#include "hw_port/ptp_port_tiva_tm4c1294.h"

#define PTP_MAIN_OSCILLATOR_FREQ_HZ (25000000)
#define PTP_INCREMENT_NSEC (50)

#define PTP_HW_INIT(increment, addend) ptphw_init(increment, addend)
#define PTP_UPDATE_CLOCK(s,ns) EMACTimestampSysTimeUpdate(EMAC0_BASE, labs(s), abs(ns), (s * NANO_PREFIX + ns) < 0)
#define PTP_SET_ADDEND(addend) EMACTimestampAddendSet(EMAC0_BASE, addend)

#include "servo/pd_controller.h"


#define PTP_SERVO_INIT() pd_ctrl_init()
#define PTP_SERVO_RESET() pd_ctrl_reset()
#define PTP_SERVO_RUN(d) pd_ctrl_run(d)

// -------------------------------------------
// (End of customizable area)
// -------------------------------------------
// Values below are autocalculated, do not overwrite them!
// -------------------------------------------

#define PTP_CLOCK_TICK_FREQ_HZ (1000000000 / PTP_INCREMENT_NSEC)
#define PTP_ADDEND_INIT ((uint32_t)(0x100000000 / (PTP_MAIN_OSCILLATOR_FREQ_HZ / (float)PTP_CLOCK_TICK_FREQ_HZ))) // initial value of addend
#define PTP_ADDEND_CORR_PER_PPB_F ((float)0x100000000 / ((float)PTP_INCREMENT_NSEC * PTP_MAIN_OSCILLATOR_FREQ_HZ))  // addend value/1ppb tuning

// -------------------------------------------

void ptp_init(struct udp_pcb * pPCBs[]); // initialize PTP subsystem
void ptp_log_en(bool en); // enable/disable logging
void ptp_log_corr_field_en(bool en); // enable/disable logging of correction fields
void ptp_set_clock_offset(int32_t offset); // set PPS offset
int32_t ptp_get_clock_offset(); // get PPS offset
void ptp_reset(); // reset PTP subsystem
void ptp_process_packet(struct pbuf * pPBuf); // process PTP packet

#endif /* PTP */
