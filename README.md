# flexPTP
## Flexible, low-resource IEEE1588 PTP slave-only implementation designed for MCU-based systems [implemented on TI TM4C1294 Connected LaunchPad]

### Prerequests

In order to compile the CCS project found in this repository the following tools are required:

- TI Code Composer Studio 9.3.0.00012 or higher,
- TI TivaWare peripheral library (this package can be aquired through CCS),
- Some modification of vendor provided drivers and files (see below).

To run the project you will need:

- a TI Connected LaunchPad,
- a network featuring a master clock.

### Binary image

If you don't want to compile the project for yourself, a precompiled binary can be downloaded from here. This binary image can be flashed easily using CCS. 

### Serial console

The project is equipped with a moderately interactive command line interface accessible through the virtual serial port (e.g. `/dev/ttyACMx` on Linux) emulated by the development board. Serial port configuration: `115200-8-N-1`. 

After the device and software has come up, type `?` to print help, a list of terminal commands. Some commands will only be available if the Ethernet network is connected.

Sample command list:

<code>

    ? 	 Print this help
    ptp servo params [Kp Kd] 			Set or query K_p and K_d servo parameters
    ptp reset 			Reset PTP subsystem
    ptp servo offset [offset_ns] 			Set or query clock offset
    ptp log {def|corr} {on|off} 			Turn on or off logging

</code>


### Usage

To try it out, just connect the Connected LaunchPad to a live Ethernet network being connected to a master clock. After the node has established connection to the network logs and servo settings are acessible through CLI. 1PPS output is accessible on pin PG0.

### Project modules

The project is buit up from multiple modules, the following are designed to be easy to replace or modify:

- servo: implementing the clock servo
- hw_port: containing setup functions for specific timestamp hardware

Instructions on how to replace the current modules can be found in `ptp.h`.

### Network driver modifications 

Original netif driver (located in `third_party/lwip-1.4.1/ports/netif/tiva-tm4c129.c`)
has been modified so that trasmitted packets being timestamped and timestamps being written back into pbufs.

Enabling TX timestamping in `tivaif_transmit(...)`

<code>

    ...

    pDesc->Desc.ui32CtrlStatus |= (DES0_TX_CTRL_IP_ALL_CKHSUMS | DES0_TX_CTRL_CHAINED);
      
<b>

    #if LWIP_PTPD
      pDesc->Desc.ui32CtrlStatus |= (1 << 25); // set TTSS flag
    #endif

</b>

    /* Decrement our descriptor counter, move on to the next buffer in the
    * pbuf chain. */
    ui32NumChained--;
    pBuf = pBuf->next;

    ...
</code>

Enabling timestamp writeback in `tivaif_process_transmit(...)`

<code>

    ...

    tDescriptorList *pDescList;
    uint32_t ui32NumDescs;

<b>    

    #if LWIP_PTPD
        struct pbuf * pPBuf;
        struct tEMACDMADescriptor * pDesc;
    #endif

</b>

    ...

</code>


<code>            

    /* Yes - free it if it's not marked as an intermediate pbuf */
            if(!((uint32_t)(pDescList->pDescriptors[pDescList->ui32Read].pBuf) & 1))
            {
                pbuf_free(pDescList->pDescriptors[pDescList->ui32Read].pBuf);
                DRIVER_STATS_INC(TXBufFreedCount);
            }


<b>

    #if LWIP_PTPD
            pPBuf = pDescList->pDescriptors[pDescList->ui32Read].pBuf;
            pDesc = &pDescList->pDescriptors[pDescList->ui32Read].Desc;
            
            // Write timestamp back
            pPBuf->time_s = pDesc->ui32IEEE1588TimeHi;
            pPBuf->time_ns = pDesc->ui32IEEE1588TimeLo;
    #endif

</b>

            pDescList->pDescriptors[pDescList->ui32Read].pBuf = NULL;

            ...
            
</code>

Another modification has been made to enbale multicast message transmission and 
reception:

(line 328:)

<code>

    psNetif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP <b>| NETIF_FLAG_IGMP</b>;

</code>

### UARTprintf modification

To enable task switching while waiting for new data to arrive on serial port, insert some (OS managed) delay into `UARTgets(...)` in `uartstdio.c`

<code>            

    ...

    //
    // Process characters until a newline is received.
    //
    while(1)
    {
        //
        // Read the next character from the receive buffer.
        //


<b>

        vTaskDelay(pdMS_TO_TICKS(10));

</b>

        if(!RX_BUFFER_EMPTY)
        {
    
    ...
            
</code>