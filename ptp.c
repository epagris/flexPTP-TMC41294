/* (C) András Wiesner, 2020 */

#include "ptp.h"
#include "utils.h"
#include "timers.h"

#include "cli.h"

// state machine states
enum FSMState
{
    SIdle, SWaitFollowUp, SWaitDelayResp
};

// global state
static struct
{
    enum FSMState state; // state
    uint16_t sequenceID, delay_reqSequenceID; // last sequency IDs

} sState;

static struct SyncCycleData sSyncData; // full dataset for performing synchronization

// PPS subsystem options
static struct
{
    struct TimestampI offset; // PPS signal offset
} sOptions;

static struct PTPHeader sDelayReqHeader; // header for sending Delay_Reg messages
static uint64_t sClockIdentity; // clockIdentity calculated from MAC address

static struct ip_addr sDefPTPAddr;  // default PTP IP address
static struct pbuf *spDelayReqPBuf; // TODO: ....
static struct udp_pcb **spPCBs;     // PCBs for sending packets

static TimerHandle_t sResponseTimer = NULL; // timer for sync dropout detection

// --------------------------

// logging
static bool log_en = false;
static bool log_corr = false;

// enable/disable general logging
void ptp_log_en(bool en)
{
    if (en && !log_en)
    {
        MSG("\n\nT1 [s] | T1 [ns] | T4 [s] | T4 [ns] | Dt [s] | Dt [ns] | Dt [tick] | Addend\n\n");
    }

    log_en = en;
}

// enable/disable logging of correction field values
void ptp_log_corr_field_en(bool en)
{
    log_corr = en;
}

// --------------------------

void ptp_reset();
void ptp_reset_state(TimerHandle_t xTimer);

// --------------------------

#define RESP_TIMEOUT (2000) // allowed maximal Delay_Resp response time
void ptp_create_response_timer()
{
    sResponseTimer = xTimerCreate("resptimer", pdMS_TO_TICKS(RESP_TIMEOUT), // timeout
                                  false, // timer operates in single shot mode
                                  (void*) 1, // ID
                                  ptp_reset_state); // callback-function
}

// print clock identity
void ptp_print_clock_identity(uint64_t clockID)
{
    uint8_t *p = (uint8_t*) &clockID;

    MSG("ClockID: ");
    uint8_t i;
    for (i = 8; i > 0; i--)
    { // reverse byte order due to Network->Host byte order conversion
        MSG("%02x", p[i - 1]);
    }
    MSG("\n");
}

// create clock identity based on MAC address
void ptp_create_clock_identity()
{
    uint8_t *p = (uint8_t*) &sClockIdentity;
    // construct clockIdentity
    memcpy(p, netif_default->hwaddr, 3); // first 3 octets of MAC address
    p[3] = 0xff;
    p[4] = 0xfe;
    memcpy(&p[5], &netif_default->hwaddr[3], 3); // last 3 octets of MAC address

    // display ID
    ptp_print_clock_identity(sClockIdentity);
}

// clear flag structure
void ptp_clear_flags(struct PTPFlags *pFlags)
{
    memset(pFlags, 0, sizeof(struct PTPFlags));
}

// initialize Delay_Req header
void ptp_init_delay_req_header()
{
    sDelayReqHeader.messageID = PTPIDDelay_Req;
    sDelayReqHeader.transportSpecific = 0;
    sDelayReqHeader.versionPTP = 2;     // PTPv2
    sDelayReqHeader.messageLength = 44;
    sDelayReqHeader.subdomainNumber = 0;
    ptp_clear_flags(&(sDelayReqHeader.flags)); // no flags
    sDelayReqHeader.correction_ns = 0;
    sDelayReqHeader.correction_subns = 0;

    memcpy(&sDelayReqHeader.clockIdentity, &sClockIdentity, 8);

    sDelayReqHeader.sourcePortID = 1;
    sDelayReqHeader.sequenceID = 0; // will change in every synch cycle
    sDelayReqHeader.control = PTPCONDelay_Req;
    sDelayReqHeader.logMessagePeriod = 0x7f;
}

// create pbuf for sending Delay_Reqs
void ptp_init_delay_req()
{
    spDelayReqPBuf = NULL;
}

static int CB_reset(const CliToken_Type *ppArgs, uint8_t argc)
{
    ptp_reset();
    MSG("> PTP reset!\n");
    return 0;
}

static int CB_offset(const CliToken_Type *ppArgs, uint8_t argc)
{
    if (argc > 0)
    {
        ptp_set_clock_offset(atoi(ppArgs[0]));
    }

    MSG("> PTP clock offset: %d ns\n", ptp_get_clock_offset());
    return 0;
}

static int CB_log(const CliToken_Type *ppArgs, uint8_t argc)
{
    bool logEn = false;

    if (argc > 1)
    {
        if (!strcmp(ppArgs[1], "on"))
        {
            logEn = true;
        }
        else if (!strcmp(ppArgs[1], "off"))
        {
            logEn = false;
        }
        else
        {
            return -1;
        }

        if (!strcmp(ppArgs[0], "def")) {
            ptp_log_en(logEn);
        } else if (!strcmp(ppArgs[0], "corr")) {
            ptp_log_corr_field_en(logEn);
        } else {
            return -1;
        }

    }
    else
    {
        return -1;
    }

    return 0;
}

// register cli commands
static void ptp_register_cli_commands()
{
    cli_register_command("ptp reset \t\t\tReset PTP subsystem", 2, 0, CB_reset);
    cli_register_command("ptp servo offset [offset_ns] \t\t\tSet or query clock offset", 3, 0, CB_offset);
    cli_register_command("ptp log {def|corr} {on|off} \t\t\tTurn on or off logging", 2, 2, CB_log);
}

// initialize PTP module
void ptp_init(struct udp_pcb *pPCBs[])
{
    // create clock identity
    ptp_create_clock_identity();

    // seed the randomizer
    srand(sClockIdentity);

    // create pbuf used to send Delay_Reqs
    ptp_init_delay_req();

    // construct header for sending Delay_Req messages
    ptp_init_delay_req_header();

    // fill in default destination IP address
    sDefPTPAddr.addr = ipaddr_addr(PTP_IGMP_DEFAULT);

    spPCBs = pPCBs;

    // reset state machine to IDLE
    sState.state = SIdle;
    sState.delay_reqSequenceID = 0;

    // reset options
    sOptions.offset.nanosec = 0;

    // initialize hardware
    PTP_HW_INIT(PTP_INCREMENT_NSEC, PTP_ADDEND_INIT);

    // initialize controller
    PTP_SERVO_INIT();

    // reset PTP subsystem
    ptp_reset();

    // create droup detecting timer
    ptp_create_response_timer();

    // register cli commands
    ptp_register_cli_commands();
}

// Network->Host byte order conversion for 64-bit values
uint64_t ntohll(uint64_t in)
{
    unsigned char out[8] = { in >> 56, in >> 48, in >> 40, in >> 32, in >> 24, in >> 16, in >> 8, in };
    return *(uint64_t*) out;
}

uint64_t htonll(uint64_t in)
{
    return ntohll(in);
}

// load ptp flags from bitfield
void ptp_load_flags(struct PTPFlags *pFlags, uint16_t bitfield)
{
    pFlags->PTP_SECURITY = (bitfield >> 15) & 1;
    pFlags->PTP_ProfileSpecific_2 = (bitfield >> 14) & 1;
    pFlags->PTP_ProfileSpecific_1 = (bitfield >> 13) & 1;

    pFlags->PTP_UNICAST = (bitfield >> 10) & 1;
    pFlags->PTP_TWO_STEP = (bitfield >> 9) & 1;
    pFlags->PTP_ALTERNATE_MASTER = (bitfield >> 8) & 1;

    pFlags->FREQUENCY_TRACEABLE = (bitfield >> 5) & 1;
    pFlags->TIME_TRACEABLE = (bitfield >> 4) & 1;

    pFlags->PTP_TIMESCALE = (bitfield >> 3) & 1;
    pFlags->PTP_UTC_REASONABLE = (bitfield >> 2) & 1;
    pFlags->PTP_LI_59 = (bitfield >> 1) & 1;
    pFlags->PTP_LI_61 = (bitfield >> 0) & 1;
}

// extract fields from a PTP header
void ptp_extract_header(struct PTPHeader *pHeader, void *pPayload)
{
    // cast header to byte accessible form
    uint8_t *p = (uint8_t*) pPayload;

    uint16_t flags;

    // copy header fields
    memcpy(&pHeader->messageID, p + 0, 1);
    memcpy(&pHeader->versionPTP, p + 1, 1);
    memcpy(&pHeader->messageLength, p + 2, 2);
    memcpy(&pHeader->subdomainNumber, p + 4, 1);
    memcpy(&flags, p + 6, 2);
    memcpy(&pHeader->correction_ns, p + 8, 8);
    memcpy(&pHeader->clockIdentity, p + 20, 8);
    memcpy(&pHeader->sourcePortID, p + 28, 2);
    memcpy(&pHeader->sequenceID, p + 30, 2);
    memcpy(&pHeader->control, p + 32, 1);
    memcpy(&pHeader->logMessagePeriod, p + 33, 1);

    pHeader->transportSpecific = 0xf0 & pHeader->messageID;
    pHeader->messageID &= 0x0f;

    // read flags
    ptp_load_flags(&pHeader->flags, ntohs(flags));

    // read correction field
    pHeader->correction_subns = ntohll(pHeader->correction_ns) & 0xffff;
    pHeader->correction_ns = ntohll(pHeader->correction_ns) >> 16;

    pHeader->messageLength = ntohs(pHeader->messageLength);
    pHeader->sourcePortID = ntohs(pHeader->sourcePortID);
    pHeader->sequenceID = ntohs(pHeader->sequenceID);
}

// construct binary header from header structure
void ptp_construct_binary_header(void *pData, struct PTPHeader *pHeader)
{
    uint8_t *p = (uint8_t*) pData;
    uint8_t firstByte;

    // host->network
    uint16_t messageLength = htons(pHeader->messageLength);
    uint16_t sourcePortID = htons(pHeader->sourcePortID);
    uint16_t sequenceID = htons(pHeader->sequenceID);

    // fill in flags FIXME
    uint16_t flags = 0; // TODO: convert from header fields

    // fill in correction value
    uint64_t correction = htonll((pHeader->correction_ns << 16) | (pHeader->correction_subns)); // TODO: ...

    // copy fields
    firstByte = (pHeader->transportSpecific << 4) | (pHeader->messageID & 0x0f);
    memcpy(p, &firstByte, 1);
    memcpy(p + 1, &pHeader->versionPTP, 1);
    memcpy(p + 2, &messageLength, 2);
    memcpy(p + 4, &pHeader->subdomainNumber, 1);
    memcpy(p + 6, &flags, 2);
    memcpy(p + 8, &correction, 8);
    memcpy(p + 20, &pHeader->clockIdentity, 8);
    memcpy(p + 28, &sourcePortID, 2);
    memcpy(p + 30, &sequenceID, 2);
    memcpy(p + 32, &pHeader->control, 1);
    memcpy(p + 33, &pHeader->logMessagePeriod, 1);
}

#define PTP_HEADER_LENGTH (34)

// extract n timestamps from a message
void ptp_extract_timestamps(struct TimestampI *ts, void *pPayload, uint8_t n)
{
    uint8_t *p = ((uint8_t*) pPayload) + PTP_HEADER_LENGTH; // pointer at the beginning of first timestamp

    // read n times
    uint8_t i;
    for (i = 0; i < n; i++)
    {
        // seconds
        ts->sec = 0;
        memcpy(&ts->sec, p, 6); // 48-bit
        p += 6;

        // nanoseconds
        memcpy(&ts->nanosec, p, 4);
        p += 4;

        // network->host
        ts->sec = ntohll(ts->sec << 16);
        ts->nanosec = ntohl(ts->nanosec);

        // step to next timestamp
        ts++;
    }
}

// wrtie n timestamps after header in to packet (TODO: subjet to further test!)
void ptp_write_binary_timestamps(void *pPayload, struct TimestampI *ts, uint8_t n)
{
    uint8_t *p = ((uint8_t*) pPayload) + PTP_HEADER_LENGTH;

    // write n times
    uint8_t i;
    for (i = 0; i < n; i++)
    {
        // get timestamp data
        uint64_t sec = htonll(ts->sec << 16);
        uint64_t nanosec = htonl(ts->nanosec);

        // fill in time data
        memcpy(p, &sec, 6); // 48-bit
        p += 6;

        memcpy(p, &nanosec, 4);
        p += 4;

        // step onto next element
        ts++;
    }
}

// construct and send Delay_Req message (NON-REENTRANT!)
void ptp_send_delay_req_message()
{
    static struct TimestampI timestamp = { 0, 0 }; // timestamp appended at the end of packet

    // release buffer if in use
    if (spDelayReqPBuf != NULL)
    {
        pbuf_free(spDelayReqPBuf);
    }

    // allocate pbuf
    spDelayReqPBuf = pbuf_alloc(PBUF_TRANSPORT, PTP_DELAY_REQ_PCKT_SIZE, PBUF_RAM);

    // increment sequenceID
    sDelayReqHeader.sequenceID = ++sState.delay_reqSequenceID;

    // fill in header
    ptp_construct_binary_header(spDelayReqPBuf->payload, &sDelayReqHeader);

    // fill in timestamp
    ptp_write_binary_timestamps(spDelayReqPBuf->payload, &timestamp, 1);

    // send message
    udp_sendto(spPCBs[0], spDelayReqPBuf, &sDefPTPAddr, PTP_PORT0);
}

// extract Delay_Resp data
void ptp_read_delay_resp_id_data(struct Delay_RespIdentification *pDRData, void *pPayload)
{
    uint8_t *p = (uint8_t*) pPayload;
    memcpy(&pDRData->requestingSourceClockIdentity, p + 44, 8);
    memcpy(&pDRData->requestingSourcePortIdentity, p + 52, 2);

    // network->host
    pDRData->requestingSourcePortIdentity = ntohs(pDRData->requestingSourcePortIdentity);
}

// set PPS offset
void ptp_set_clock_offset(int32_t offset)
{
    nsToTsI(&sOptions.offset, offset);
}

// get PPS offset
int32_t ptp_get_clock_offset()
{
    return nsI(&sOptions.offset);
}

// value of addend register
static uint32_t addend;

// reset PTP subsystem
void ptp_reset()
{
    // reset state machine
    sState.state = SIdle;
    sState.delay_reqSequenceID = 0;

    // reset addend to initial value
    addend = PTP_ADDEND_INIT;

    // reset controller
    PTP_SERVO_RESET();
}

// perform clock correction based on gathered timestamps (NON-REENTRANT!)
void ptp_perform_correction()
{
    // timestamps and time intervals
    struct TimestampI d, syncMa, syncSl, delReqSl, delReqMa;

    // copy timestamps to assign then with meaningful names
    syncMa = sSyncData.t1;
    syncSl = sSyncData.t2;
    delReqSl = sSyncData.t3;
    delReqMa = sSyncData.t4;

    // compute difference between master and slave clock
    subTime(&d, &syncSl, &syncMa); // t2 - t1 ...
    subTime(&d, &d, &delReqMa); // - t4 ...
    addTime(&d, &d, &delReqSl); // + t3
    divTime(&d, &d, 2); // division by 2

    // substract offset
    subTime(&d, &d, &sOptions.offset);

    // normalize time difference (eliminate malformed time value issues)
    normTime(&d);

    // translate time difference into clock tick unit
    int32_t d_ticks = tsToTick(&d, PTP_CLOCK_TICK_FREQ_HZ);

    // if time difference is at least one second, then jump the clock
    if (d.sec != 0)
    {
        PTP_UPDATE_CLOCK(d.sec, d.nanosec); // jump the clock by difference

        MSG("Time difference is over 1s, performing coarse correction!\n");
        return;
    }

    // run controller
    float corr_ppb = PTP_SERVO_RUN(nsI(&d));

    // compute addend value
    addend += corr_ppb * PTP_ADDEND_CORR_PER_PPB_F;

    // write addend into hardware
    PTP_SET_ADDEND(addend);

    // log on cli (if enabled)
    CLILOG(log_en, "%d %d %d %d %d %d %d 0x%X\n", (int32_t )syncMa.sec, syncMa.nanosec, (int32_t )delReqMa.sec, delReqMa.nanosec, (int32_t ) d.sec, d.nanosec, d_ticks, addend);

}

// reset state machine if message dropuot occures
void ptp_reset_state(TimerHandle_t xTimer)
{
    sState.state = SIdle;
    MSG("Response timeout expired, state machine has been reset!\n");
}

// packet processing (NON-REENTRANT!!)
void ptp_process_packet(struct pbuf *pPBuf)
{
    static struct PTPHeader header;                      // PTP header
    static struct Delay_RespIdentification delay_respID; // identification received in every Delay_Resp packet
    struct TimestampI correctionField;

    // header readout
    ptp_extract_header(&header, pPBuf->payload);

    //MSG("%d\n", header.messageID);

    switch (sState.state)
    {
    // wait for Sync message
    case SIdle:
    {
        // switch into next state if Sync packet has arrived
        if (header.messageID == PTPIDSync)
        {
            // save reception time
            sSyncData.t2.sec = pPBuf->time_s;
            sSyncData.t2.nanosec = pPBuf->time_ns;

            // TODO: TWO_STEP handling

            // switch to next state
            sState.sequenceID = header.sequenceID;
            sState.state = SWaitFollowUp;

            // start dropout timer
            xTimerStart(sResponseTimer, 0);

#if PRINT_STATE_TRANSITION_MESSAGES
            MSG("IDLE -> WAITFOLLOWUP\n");
#endif
        }

        break;
    }

        // wait for Follow_Up message
    case SWaitFollowUp:
        if (header.messageID == PTPIDFollow_Up)
        {
            // check sequence ID if the response is ours
            if (header.sequenceID == sState.sequenceID)
            {
                // read t1
                ptp_extract_timestamps(&sSyncData.t1, pPBuf->payload, 1);

                // get correction field (substract from t2)
                correctionField.sec = 0;
                correctionField.nanosec = header.correction_ns; // TODO: subnanosec processing

                subTime(&sSyncData.t2, &sSyncData.t2, &correctionField);
                normTime(&sSyncData.t2);

                // log correction field (if enabled)
                CLILOG(log_corr, "C [Follow_Up]: %d\n", correctionField.nanosec);

                // delay Delay_Req transmission with a random amount of time
                vTaskDelay(pdMS_TO_TICKS(rand() % 500));

                // send Delay_Req message
                ptp_send_delay_req_message();

                // switch to next state
                sState.state = SWaitDelayResp;

#if PRINT_STATE_TRANSITION_MESSAGES
                MSG("WAITFOLLOWUP -> WAITDELAYRESP\n");
#endif
            }
            /*else
             {
             sState.state = SIdle; // error, switch to IDLE
             }*/
        }

        break;

        // wait for Delay_Resp message
    case SWaitDelayResp:
        if (header.messageID == PTPIDDelay_Resp && header.sequenceID == sState.delay_reqSequenceID)
        {

            // read clock ID of requester
            ptp_read_delay_resp_id_data(&delay_respID, pPBuf->payload);

            // if sent to us as a response to our Delay_Req then continue processing
            if (delay_respID.requestingSourceClockIdentity == sClockIdentity && delay_respID.requestingSourcePortIdentity == sDelayReqHeader.sourcePortID)
            {
                // print clockID
                /*ptp_print_clock_identity(
                 delay_respID.requestingSourceClockIdentity);*/

                // store t3
                sSyncData.t3.sec = spDelayReqPBuf->time_s;
                sSyncData.t3.nanosec = spDelayReqPBuf->time_ns;

                // store t4
                ptp_extract_timestamps(&sSyncData.t4, pPBuf->payload, 1);

                // substract correction field from t4
                correctionField.sec = 0;
                correctionField.nanosec = header.correction_ns;

                subTime(&sSyncData.t4, &sSyncData.t4, &correctionField);
                normTime(&sSyncData.t4);

                // log correction field (if enabled)
                CLILOG(log_corr, "C [Del_Resp]: %d\n", correctionField.nanosec);

                // run clock correction algorithm
                ptp_perform_correction();

                // switch to IDLE state
                sState.state = SIdle;

                // stop dropout detecting timer
                xTimerStop(sResponseTimer, 0);

#if PRINT_STATE_TRANSITION_MESSAGES
                MSG("WAITDELAYRESP -> IDLE\n\n");
#endif
            }
        }

        break;
    }
}
