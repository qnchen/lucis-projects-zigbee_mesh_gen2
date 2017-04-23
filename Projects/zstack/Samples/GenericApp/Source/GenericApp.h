/**************************************************************************************************
  Filename:       GenericApp.h
  Revised:        $Date: 2012-02-12 16:04:42 -0800 (Sun, 12 Feb 2012) $
  Revision:       $Revision: 29217 $

  Description:    This file contains the Generic Application definitions.


  Copyright 2004-2012 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License"). You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product. Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE, 
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com. 
**************************************************************************************************/

#ifndef GENERICAPP_H
#define GENERICAPP_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"
#include "ZDApp.h"


/*********************************************************************
 * CONSTANTS
 */
#define ZIGBEE_FW_VERSION_LEN   10
#define ZIGBEE_FW_VERSION      "v01.00.008" //xx.yy.zzz


// These constants are only for example and should be changed to the
// device's needs
#define GENERICAPP_ENDPOINT           10

#define GENERICAPP_PROFID             0x0F04
#define GENERICAPP_DEVICEID           0x0001
#define GENERICAPP_DEVICE_VERSION     0
#define GENERICAPP_FLAGS              0

#define GENERICAPP_MAX_CLUSTERS                            7
#define GENERICAPP_CLUSTERID          			          1
#define GENERICAPP_DATA_P2P_CLUSTERID 	          2
#define GENERICAPP_GROUP_CLUSTERID                      3
#define GENERICAPP_CMD_P2P_CLUSTERID                  4
#define GENERICAPP_BROADCAST_CLUSTERID              5
#define GENERICAPP_P2P_PERMIT_JOIN_REQ_CLUSTERID        6 // Router permit for join from coordinator
#define GENERICAPP_P2P_PERMIT_JOIN_RSP_CLUSTERID        7 // Router permit for join from coordinator


// Send Message Timeout
#define GENERICAPP_SEND_MSG_TIMEOUT   5000     // Every 5 seconds

// Application Events (OSAL) - These are bit weighted definitions.
#define GENERICAPP_SEND_MSG_EVT       0x0001

#if defined( IAR_ARMCM3_LM )
#define GENERICAPP_RTOS_MSG_EVT       0x0002
#endif  


#define GENERICAPP_PING_MSG_EVT                 0x0004
#define GENERICAPP_LINK_STATUS_EVT            0x0008
#define GENERICAPP_CHANNEL_MSG_EVT          0x0010
#define GENERICAPP_RESET_SB_MSG_EVT         0x0020
#define GENERICAPP_LOADSTATUS_MSG_EVT    0x0040

#define GENERICAPP_FEED_DOG_EVT                                 0x0400
#define GENERICAPP_COORDINATOR_ONLINE_EVT             0x0800
#define GENERICAPP_ROUTER_ONLINE_EVT                       0x1000

#define GENERICAPP_PERMIT_JOIN_MSG_EVT        0x2000   //chaokw
#define GENERICAPP_CLEAR_JOIN_DEVICES_EVT   0x4000
#define GENERICAPP_PING_TIMEOUT_CHECK_EVT  0x0080
#define GENERICAPP_COORDINATER_BROADCAST_PING_EVT  0x0100


#define GENERICAPP_FEED_DOG_TIME_MAX      800  //chaokw watchdog

#define TRANSDIR_UPLINK		0
#define TRANSDIR_DOWNLINK		1

#define GENERICAPP_CLEAR_JOIN_DEVICE_TIMEOUT    150000 // 2.5min

/*********************************************************************
 * MACROS
 */
#define nodeStatus_SUCCESS            ZSuccess           /* 0x00 */
#define nodeStatus_FAILED             ZFailure           /* 0x01 */
#define nodeStatus_INVALID_PARAMETER  ZInvalidParameter  /* 0x02 */
#define nodeStatus_MEM_FAIL           ZMemError          /* 0x10 */
#define nodeStatus_NO_ROUTE           ZNwkNoRoute        /* 0xCD */

#define BREAK_UINT16( var, ByteNum ) \
    (uint8)( (uint16)( ( (var) >>( (ByteNum) * 8 ) ) & 0x00FF ) )



typedef ZStatus_t nodeStatus_t;


/*   */
#define PING_PERIOD                    15000 
#define PING_TIMEOUT                 PING_PERIOD * TIMEOUT_CNT
#define TIMEOUT_CNT                   4
#define LINKSTAT_PRRIOD            PING_PERIOD
#define PING_TIMEOUT_CHECK     10000   //chaokw

#define MT_UART_HEAD_LEN	     3
#define DEFAULTFCS 	                   0xC5

/*  lucis protocol */
#define MT_CPT_SOP                               0xFE
#define MT_ZIGBEE_START_IND		 0x1680
#define MT_LOAD_STATUS_IND	        0x1780

#define MT_NODE_REG_IND			 0x1880
#define MT_NODE_DEREG_IND		        0x1980


#define MT_DATA_P2P_REQ                    0x2001
#define MT_DATA_P2P_RSP                    0x2080
#define MT_BROADCAST_REQ                  0x2301
#define MT_BROADCAST_RSP                  0x2380


#define MT_WIFI_CHANNEL_REQ            0x3480
#define MT_WIFI_CHANNEL_RSP            0x3401
#define MT_MAC_REQ_MSG                     0x3501 
#define MT_MAC_RSP_MSG                     0x3580
#define MT_FW_VERSION_REQ_MSG       0x3601
#define MT_FW_VERSION_RSP_MSG       0x3680 
#define MT_FW_UPDATE_REQ_MSG        0x3701 
#define MT_FW_UPDATE_RSP_MSG        0x3780 
#define MT_ACTIVE_CNT_REQ_MSG        0x4101 
#define MT_ACTIVE_CNT_RSP_MSG        0x4180 
#define MT_MULTIWAY_SET_REQ_MSG   0x4201 
#define MT_MULTIWAY_SET_RSP_MSG   0x4280 
#define MT_COORDINATOR_ONLINE_REQ_MSG        0x4401 

#define MT_NWKINFO_RSP_MSG              0x9080



#define PING_REQ_MSG                           0x103A
#define PING_RSP_MSG                           0x103B


#define REPORT_RPTMAC_MSG		        0x602C
#define GROUP_LIGHT_STATE_MSG		 0x207A
#define COORDINATOR_ONLINE_REQ       0x211A
#define ROUTER_ONLINE_REQ                  0x211A
#define ROUTER_ONFFLINE_REQ              0x211F

#define PROT_JOIN_REQ_MSG            0x304A  // Route transfer the join device info to coordinator for premission
#define PROT_JOIN_RSP_MSG            0x304B  // Coordinator rsp Router the premission

//DATA TRANSFER PROTOCOL:FCS = msg id ^ dest_id ^ src_id ^ datalen ^ data
//| MSG ID | MSG_SEQ  |DEST_ID | SRC_ID | DATALEN | DATA | FCS |
//| 2	    |       2	  |      2      |      2      |       1       |   N     |   1   |    
#define PKT_HEAD_LEN	9


typedef enum
{
    nodedisconnected = 1,
    nodeconnected
} e_Status;

typedef struct _nodeList_t {
    struct _nodeList_t *nextDesc;
    afAddrType_t devAddr64;
    afAddrType_t devAddr16;
    //uint8 capabilities;
    e_Status status;
    uint8 sequence;
    uint32 timestamp;
    uint32 oldtimestamp;
    uint8 timeoutcnt;
} nodeList_t;


//======================================================
//DATA TRANSFER PROTOCOL:FCS = msg id ^ dest_id ^ src_id ^ datalen ^ data
//| MSG ID | MSG_SEQ  | DEST_ID | SRC_ID | DATALEN | DATA | FCS |
//| 2	    |       2         |       2      |      2      |       1       |   N     |   1   |   
typedef struct
{
    uint16 msgid;
    uint16 msgseq;	
    uint16 destid;
    uint16 srcid;
    uint8 datalen;
    uint8 *data;
    uint8 fcs;
}transferpkt_t;


typedef struct
{	
    uint16 msgid;
    afAddrType_t devAddr64;	
    afAddrType_t devAddr16;		
    uint16 parentaddr;
    uint32 timestamp;
    uint8 seqnum;   	    	
}pingpkt_t;



extern const uint8* fw_version;
extern byte GenericApp_TaskID;
extern byte GenericApp_TransID; 
extern endPointDesc_t GenericApp_epDesc;
extern afAddrType_t SerialApp_TxAddr_p2p;
extern afAddrType_t SerialApp_TxAddr_broadcast;
extern afAddrType_t SerialApp_TxAddr_group;

extern uint8 transfer_join_times;


/*********************************************************************
 * FUNCTIONS
 */



/*
 * Task Initialization for the Generic Application
 */
extern void GenericApp_Init( byte task_id );

/*
 * Task Event Processor for the Generic Application
 */
extern UINT16 GenericApp_ProcessEvent( byte task_id, UINT16 events );


extern nodeList_t *CustomApp_NodeRegister (afAddrType_t addr64, afAddrType_t addr16);
extern nodeStatus_t CustomApp_NodeDelete ( uint16 devaddr );
extern nodeList_t *CustomApp_NodeFindNwkAddrList ( uint16 Addr16 );
extern nodeList_t *CustomApp_NodeFindMacAddrList ( uint8* Addr64 );
extern void CustomApp_ping_request_init( void );
extern void CustomApp_ping_timeout_check_init( void );   //chaokw ping
extern void CustomApp_coordinater_broadcast_ping_init( void );
extern void CustomApp_zigbee_start_ind (void);
extern void CustomApp_loadstatus_ind(void);
extern void CustomApp_node_register_ind(afAddrType_t addr64, afAddrType_t addr16);
extern void CustomApp_node_deregister_ind(afAddrType_t addr64, afAddrType_t addr16);
extern void CustomApp_send_ping_req (void);
extern void CustomApp_send_ping_rsp ( uint16 Addr16 );  
extern void CustomApp_coordinater_broadcast_ping( void );
extern void CustomApp_process_ping( pingpkt_t *pkt  );
extern void CustomApp_ping_timeout_check ( void );
extern void CustomApp_linkstatus_updated ( void );
extern void CustomApp_linkstatus_init( void );
extern uint8 CustomApp_get_connected_node_count( void );
extern bool GenericApp_Device_JoinRsp(afIncomingMSGPacket_t *pkt);
extern void GenericApp_Get_PermitJoin(afIncomingMSGPacket_t *pkt);
extern bool GenericApp_Device_JoinReq(void);
extern void GenericApp_Free_Join_DeviceInfo(ZDO_Permit_Join_Device_t *permit_join_device);


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* GENERICAPP_H */
