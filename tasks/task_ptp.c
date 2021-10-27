#include "FreeRTOS.h"
#include "task.h"

#include "user_tasks.h"

#include "ptp.h"

#include "lwip/igmp.h"

// ----- TASK PROPERTIES -----
static TaskHandle_t sTH; // task handle
static uint8_t sPrio = 5; // priority
static uint16_t sStkSize = 8192; // stack size
void task_ptp(void * pParam); // task routine function
// ---------------------------

// udp control blocks
static struct udp_pcb * spPTP_pcb[2];

// callback function receiveing data from udp "sockets"
void ptp_recv_cb(void * pArg, struct udp_pcb * pPCB, struct pbuf *pP, ip_addr_t * pAddr, uint16_t port);

// FIFO for incoming packets
#define PACKET_FIFO_LENGTH (32)
static QueueHandle_t sPacketFIFO;

// create udp listeners
void create_ptp_listeners() {
    // create packet FIFO
    sPacketFIFO = xQueueCreate(PACKET_FIFO_LENGTH, sizeof(struct pbuf *));

    // listening on the port 319
    spPTP_pcb[0] = udp_new();
    udp_bind(spPTP_pcb[0], IP_ADDR_ANY, PTP_PORT0);
    udp_recv(spPTP_pcb[0], ptp_recv_cb, NULL);

    // listening on the port 320
    spPTP_pcb[1] = udp_new();
    udp_bind(spPTP_pcb[1], IP_ADDR_ANY, PTP_PORT1);
    udp_recv(spPTP_pcb[1], ptp_recv_cb, NULL);
}

// remove listeners
void destroy_ptp_listeners() {
	// disconnect UDP "sockets"
    udp_disconnect(spPTP_pcb[0]);
    udp_disconnect(spPTP_pcb[1]);

    // destroy UDP sockets
    udp_remove(spPTP_pcb[0]);
    udp_remove(spPTP_pcb[1]);

    // destroy packet FIFO
    vQueueDelete(sPacketFIFO);
}

// join PTP IGMP groups
void join_ptp_igmp_groups() {
    // join group for default set of messages (everything except for peer delay)
    struct ip_addr addr_PTP_IGMP = { ipaddr_addr(PTP_IGMP_DEFAULT) }; 
    igmp_joingroup(&netif_default->ip_addr, &addr_PTP_IGMP);

    // join group of peer delay messages
    addr_PTP_IGMP.addr = ipaddr_addr(PTP_IGMP_PEER_DELAY);
    igmp_joingroup(&netif_default->ip_addr, &addr_PTP_IGMP);
}

// leave PTP IGMP group
void leave_ptp_igmp_groups() {
    // leave default group
    struct ip_addr addr_PTP_IGMP = { ipaddr_addr(PTP_IGMP_DEFAULT) };
    igmp_leavegroup(&netif_default->ip_addr, &addr_PTP_IGMP);

    // leave group of peer delay messages
    addr_PTP_IGMP.addr = ipaddr_addr(PTP_IGMP_PEER_DELAY);
    igmp_leavegroup(&netif_default->ip_addr, &addr_PTP_IGMP);
}

// register PTP task and initialize
void reg_task_ptp() {
    join_ptp_igmp_groups(); // enter PTP IGMP groups
    create_ptp_listeners(); // create listeners

    ptp_init(spPTP_pcb); // initialize PTP subsystem

    // create task
    BaseType_t result = xTaskCreate(task_ptp, "PTP_usr", sStkSize, NULL, sPrio, &sTH);
    if (result != pdPASS) {
    	MSG("Failed to create task! (errcode: %d)\n", result);
    }
}

// unregister PTP task
void unreg_task_ptp() {
	vTaskDelete(sTH); // taszk törlése

	leave_ptp_igmp_groups(); // kilépés az IGMP csoportokból
	destroy_ptp_listeners(); // figyelők törlése
}

// callback for packet reception on port 319 and 320
void ptp_recv_cb(void * pArg, struct udp_pcb * pPCB, struct pbuf *pP, ip_addr_t * pAddr, uint16_t port) {
    xQueueSend(sPacketFIFO, &pP, portMAX_DELAY);
}

// taszk függvénye
void task_ptp(void * pParam) {
    // pointer of received packet (assigned subsequently)
    struct pbuf * pPBuf;
    
    while (1) {
        // pop packet from FIFO
        xQueueReceive(sPacketFIFO, &pPBuf, portMAX_DELAY);

        // process packet
        ptp_process_packet(pPBuf);
        
        // release pbuf resources
        pbuf_free(pPBuf);
    }
}


