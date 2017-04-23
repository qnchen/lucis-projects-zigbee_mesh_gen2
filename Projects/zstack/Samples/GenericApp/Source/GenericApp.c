/******************************************************************************
  Filename:       GenericApp.c
  Revised:        $Date: 2014-09-07 13:36:30 -0700 (Sun, 07 Sep 2014) $
  Revision:       $Revision: 40046 $

  Description:    Generic Application (no Profile).


  Copyright 2004-2014 Texas Instruments Incorporated. All rights reserved.

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
******************************************************************************/

/*********************************************************************
  This application isn't intended to do anything useful - it is
  intended to be a simple example of an application's structure.

  This application periodically sends a "Hello World" message to
  another "Generic" application (see 'txMsgDelay'). The application
  will also receive "Hello World" packets.

  This application doesn't have a profile, so it handles everything
  directly - by itself.

  Key control:
    SW1:  changes the delay between TX packets
    SW2:  initiates end device binding
    SW3:
    SW4:  initiates a match description request
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "GenericApp.h"
#include "DebugTrace.h"

#if !defined( WIN32 ) || defined( ZBIT )
  #include "OnBoard.h"
#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"

#include "MT_UART.h"
#include "OSAL_Clock.h"

#include "ZDSecMgr.h"
#include "ProtocolHandler.h"
#include "NodeAuth.h"

#include "stdlib.h"

/* RTOS */
#if defined( IAR_ARMCM3_LM )
#include "RTOS_App.h"
#endif

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
// This list should be filled with Application specific Cluster IDs.
const cId_t GenericApp_ClusterList[GENERICAPP_MAX_CLUSTERS] =
{
  GENERICAPP_CLUSTERID,					  
  GENERICAPP_DATA_P2P_CLUSTERID, 		  
  GENERICAPP_GROUP_CLUSTERID,		  
  GENERICAPP_CMD_P2P_CLUSTERID,			  
  GENERICAPP_BROADCAST_CLUSTERID,
  GENERICAPP_P2P_PERMIT_JOIN_REQ_CLUSTERID,
  GENERICAPP_P2P_PERMIT_JOIN_RSP_CLUSTERID
};

const SimpleDescriptionFormat_t GenericApp_SimpleDesc =
{
  GENERICAPP_ENDPOINT,              //  int Endpoint;
  GENERICAPP_PROFID,                //  uint16 AppProfId[2];
  GENERICAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  GENERICAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  GENERICAPP_FLAGS,                 //  int   AppFlags:4;
  GENERICAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)GenericApp_ClusterList,  //  byte *pAppInClusterList;
  GENERICAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)GenericApp_ClusterList   //  byte *pAppInClusterList;
};

// This is the Endpoint/Interface description.  It is defined here, but
// filled-in in GenericApp_Init().  Another way to go would be to fill
// in the structure here and make it a "const" (in code space).  The
// way it's defined in this sample app it is define in RAM.
endPointDesc_t GenericApp_epDesc;


nodeList_t *nodeList = NULL;



/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
byte GenericApp_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // GenericApp_Init() is called.

devStates_t GenericApp_NwkState;

byte GenericApp_TransID;  // This is the unique message ID (counter)

afAddrType_t GenericApp_DstAddr;
afAddrType_t SerialApp_TxAddr_broadcast;
afAddrType_t SerialApp_TxAddr_group;
afAddrType_t SerialApp_TxAddr_p2p;


uint8 ping_result;
uint8 ping_error_cnt;

uint8 zc_online_report_cnt =0;
uint8 zr_online_report_cnt =0;


// Number of recieved messages
static uint16 rxMsgCount;

#if defined( LCD_SUPPORTED )		
static uint16 askWhiteListCount;   //chaokw panid
#endif

// Time interval between sending messages
static uint32 txMsgDelay = GENERICAPP_SEND_MSG_TIMEOUT;

const uint8* fw_version = ZIGBEE_FW_VERSION;

uint8 transfer_join_times = 0;  //chaokw

#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)		
uint32 pingRspTime = 0;   //chaokw ping
#endif

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
static void GenericApp_HandleKeys( byte shift, byte keys );
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
static void GenericApp_SendTheMessage( void );

#if defined( IAR_ARMCM3_LM )
static void GenericApp_ProcessRtosMessage( void );
#endif

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      GenericApp_Init
 *
 * @brief   Initialization function for the Generic App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void GenericApp_Init( uint8 task_id )
{
  GenericApp_TaskID = task_id;
  GenericApp_NwkState = DEV_INIT;
  GenericApp_TransID = 0;

  // Device hardware initialization can be added here or in main() (Zmain.c).
  // If the hardware is application specific - add it here.
  // If the hardware is other parts of the device add it in main().

  GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  GenericApp_DstAddr.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_DstAddr.addr.shortAddr = 0;

  // Fill out the endpoint description.
  GenericApp_epDesc.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_epDesc.task_id = &GenericApp_TaskID;
  GenericApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&GenericApp_SimpleDesc;
  GenericApp_epDesc.latencyReq = noLatencyReqs;


    //broadcast to everyone
    SerialApp_TxAddr_broadcast.addrMode = (afAddrMode_t)AddrBroadcast;
    SerialApp_TxAddr_broadcast.endPoint = GENERICAPP_ENDPOINT;
    SerialApp_TxAddr_broadcast.addr.shortAddr = 0xFFFF;

    //send to group
    SerialApp_TxAddr_group.addrMode = (afAddrMode_t)AddrGroup;
    SerialApp_TxAddr_group.endPoint = GENERICAPP_ENDPOINT;
    SerialApp_TxAddr_group.addr.shortAddr = 0x0000;
	
    //point  to point
    SerialApp_TxAddr_p2p.addrMode = (afAddrMode_t)Addr16Bit;
    SerialApp_TxAddr_p2p.endPoint = GENERICAPP_ENDPOINT;
    SerialApp_TxAddr_p2p.addr.shortAddr = 0x0000;


  // Register the endpoint description with the AF
  afRegister( &GenericApp_epDesc );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( GenericApp_TaskID );

  // Update the display
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "GenericApp", HAL_LCD_LINE_1 );
#endif

  ZDO_RegisterForZDOMsg( GenericApp_TaskID, End_Device_Bind_rsp );
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, Match_Desc_rsp );
  
#if( ZSTACK_DEVICE_BUILD == DEVICE_BUILD_COORDINATOR)
    ZDO_RegisterForZDOMsg( GenericApp_TaskID, Device_annce );  //chaokw
#endif 


#if defined( IAR_ARMCM3_LM )
  // Register this task with RTOS task initiator
  RTOS_RegisterApp( task_id, GENERICAPP_RTOS_MSG_EVT );
#endif

    CustomApp_zigbee_start_ind(); //chaokw lucis	

#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)	
    Register_Group_Multiway();
#endif
    
#if( ZSTACK_DEVICE_BUILD == DEVICE_BUILD_COORDINATOR)
    CustomApp_loadstatus_ind();
    CustomApp_linkstatus_init();   //chaokw debug


    //CustomApp_coordinater_broadcast_ping();
	
#if  NODE_AUTH 	
    Node_auth_init();
#endif    
#endif    

#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
    //CustomApp_WifiChannelReq();
#endif

#ifdef WDT_IN_PM1
  osal_start_timerEx( GenericApp_TaskID,
                      GENERICAPP_FEED_DOG_EVT,
                      GENERICAPP_FEED_DOG_TIME_MAX);
#endif

}

/*********************************************************************
 * @fn      GenericApp_ProcessEvent
 *
 * @brief   Generic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
uint16 GenericApp_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  afDataConfirm_t *afDataConfirm;

  // Data Confirmation message fields
  byte sentEP;
#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR) 
  ZStatus_t sentStatus;
#endif 
  byte sentTransID;       // This should match the value sent
  (void)task_id;  // Intentionally unreferenced parameter

  if (events & GENERICAPP_FEED_DOG_EVT)
  {
#ifdef WDT_IN_PM1
        WatchDogFeedDog();
        osal_start_timerEx( GenericApp_TaskID,
                      GENERICAPP_FEED_DOG_EVT,
                      GENERICAPP_FEED_DOG_TIME_MAX);
#endif
        return (events ^ GENERICAPP_FEED_DOG_EVT);
  }

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZDO_CB_MSG:
          GenericApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          GenericApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case AF_DATA_CONFIRM_CMD:	
          // This message is received as a confirmation of a data packet sent.
          // The status is of ZStatus_t type [defined in ZComDef.h]
          // The message fields are defined in AF.h
          afDataConfirm = (afDataConfirm_t *)MSGpkt;

          sentEP = afDataConfirm->endpoint;
          (void)sentEP;  // This info not used now
          sentTransID = afDataConfirm->transID;
          (void)sentTransID;  // This info not used now

#if 1   //chaokw ping
#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)		
          sentStatus = afDataConfirm->hdr.status;
          // Action taken when confirmation is received.
          if ( sentStatus != ZSuccess )
          {
            // The data wasn't delivered -- Do something
#if defined( LCD_SUPPORTED )
                HalLcdWriteStringValue( (char*)"aps ack:", sentStatus, 16, HAL_LCD_LINE_2 );  //chaokw		
#endif

//---------------------------------------
#if 0
                if(0x0000 == _NIB.nwkCoordAddress)
                {
                   if(sentStatus == ZMacNoACK)
                     ping_error_cnt++;
                }
		  else
		  {
		     if(sentStatus == ZApsNoAck)
                     ping_error_cnt++;
		  }
#else
                ping_error_cnt++;
#endif
//----------------------------------------

                if(ping_error_cnt < TIMEOUT_CNT)
                {
		      //SerialApp_ping_request_init();
                }
		  else if(ping_error_cnt > TIMEOUT_CNT)
		  {
                    ping_error_cnt = TIMEOUT_CNT;
		      ping_result = 1;
                    // rejoin network by zstack?
#if 0  //chaokw
		      // restart and clear NV mwk info
#if defined(NV_INIT) && defined(NV_RESTORE)
                    NLME_InitNV();
                    NLME_SetDefaultNV();
                    ZDSecMgrClearNVKeyValues();
#endif
                    Onboard_soft_reset();	
#else
                    //Onboard_soft_reset();	 //chaokw ping
#endif
		  }
          }
          else
          {
                    ping_error_cnt = 0;    //chaokw 20170412
                    ping_result = 0;
	   }
#if defined( LCD_SUPPORTED )		  
          HalLcdWriteStringValue( (char*)"pingresult", ping_result, 10, HAL_LCD_LINE_1 );
#endif
#endif
#endif

          break;

        case AF_INCOMING_MSG_CMD:
          GenericApp_MessageMSGCB( MSGpkt );
          break;

        case ZDO_STATE_CHANGE:
          GenericApp_NwkState = (devStates_t)(MSGpkt->hdr.status);

          if ((_NIB.nwkPanId >= (ZDAPP_CONFIG_PAN_ID + PANID_INCREASE_CNT))   //chaokw panid
            || (zgConfigPANID >= (ZDAPP_CONFIG_PAN_ID + PANID_INCREASE_CNT)))
          {
  #if defined(NV_INIT) && defined(NV_RESTORE)
              NLME_InitNV();
              NLME_SetDefaultNV();
              ZDSecMgrClearNVKeyValues();
  #endif
              Onboard_soft_reset();
          }            


#if defined( LCD_SUPPORTED )			       
             HalLcdWriteStringValue( (char*)"panid:", _NIB.nwkPanId, 16, HAL_LCD_LINE_1 ); 
             HalLcdWriteStringValue( (char*)"devState:", devState, 10, HAL_LCD_LINE_3 ); 			 
#endif			  
		  
          if ( (GenericApp_NwkState == DEV_ROUTER) ||
               (GenericApp_NwkState == DEV_END_DEVICE) )
          {
	      CustomApp_loadstatus_ind();	  
	      CustomApp_ping_request_init();
             CustomApp_ping_timeout_check_init();  //chaokw ping		  
             CustomApp_send_ping_req();  //chaokw  20170322	
             osal_start_timerEx( GenericApp_TaskID,
					GENERICAPP_LOADSTATUS_MSG_EVT,
					5000); 		 
          }
		  
          if ( (GenericApp_NwkState == DEV_ZB_COORD) )
          {
             CustomApp_coordinator_online_req();
             osal_start_timerEx( GenericApp_TaskID,
					GENERICAPP_COORDINATOR_ONLINE_EVT,
					5000); 			 
             CustomApp_coordinater_broadcast_ping_init();  //chaokw ping			 
          }		 
          break;


#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
        case ZDO_CHANNEL_UPDATE:
          CustomApp_ChannelSelect(MSGpkt);
	   break;
#endif
		  
        case ZDO_P2P_REQ:
          CustomApp_Send_P2P_Data(MSGpkt);
          break;

        case ZDO_GROUP_REQ:
          CustomApp_Send_Group_Data(MSGpkt);
          break;
		  
        case ZDO_BROADCAST_REQ:
          CustomApp_Send_BroadCast_Data(MSGpkt);
          break;			  

        case ZDO_MAC_REQ:
          CustomApp_Get_ExtAddr();
          break;

        case ZDO_NWKINFO_REQ:
          CustomApp_Get_NWKInfo();
          break;

        case ZDO_ACTIVE_CNT_REQ:
          CustomApp_Get_Active_Cnt();
          break;

        case ZDO_FW_VERSION_REQ:
          CustomApp_Get_Version();
          break;

        case ZDO_FW_UPDATE_REQ:
          CustomApp_FW_Update();
          break;

        case ZDO_MULTIWAY_SET_REQ:
          CustomApp_Set_Multiway(MSGpkt);
          break;
		  
        default:
          break;
      }
      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  // Send a message out - This event is generated by a timer
  //  (setup in GenericApp_Init()).
  if ( events & GENERICAPP_SEND_MSG_EVT )
  {
    // Send "the" message
    GenericApp_SendTheMessage();  //chaokw

    // Setup to send message again
    osal_start_timerEx( GenericApp_TaskID,
                        GENERICAPP_SEND_MSG_EVT,
                        txMsgDelay );

    // return unprocessed events
    return (events ^ GENERICAPP_SEND_MSG_EVT);
  }

#if defined( IAR_ARMCM3_LM )
  // Receive a message from the RTOS queue
  if ( events & GENERICAPP_RTOS_MSG_EVT )
  {
    // Process message from RTOS queue
    GenericApp_ProcessRtosMessage();

    // return unprocessed events
    return (events ^ GENERICAPP_RTOS_MSG_EVT);
  }
#endif


   if ( events & GENERICAPP_PING_MSG_EVT)
   {
	osal_stop_timerEx( GenericApp_TaskID, GENERICAPP_PING_MSG_EVT );
	CustomApp_send_ping_req();
	osal_start_timerEx( GenericApp_TaskID, GENERICAPP_PING_MSG_EVT, PING_PERIOD);

	return (events ^ GENERICAPP_PING_MSG_EVT);	
   }


   if ( events & GENERICAPP_LINK_STATUS_EVT)
   {
	osal_stop_timerEx( GenericApp_TaskID, GENERICAPP_LINK_STATUS_EVT );
	CustomApp_linkstatus_updated();
	osal_start_timerEx( GenericApp_TaskID, GENERICAPP_LINK_STATUS_EVT, LINKSTAT_PRRIOD);

	return (events ^ GENERICAPP_LINK_STATUS_EVT);	
   }

#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)	
   if ( events & GENERICAPP_PING_TIMEOUT_CHECK_EVT)   //chaokw ping
   {
	osal_stop_timerEx( GenericApp_TaskID, GENERICAPP_PING_TIMEOUT_CHECK_EVT );
	CustomApp_ping_timeout_check();
	osal_start_timerEx( GenericApp_TaskID, GENERICAPP_PING_TIMEOUT_CHECK_EVT, PING_TIMEOUT_CHECK);

	return (events ^ GENERICAPP_PING_TIMEOUT_CHECK_EVT);	
   }   
#endif

   if ( events & GENERICAPP_COORDINATER_BROADCAST_PING_EVT)  //chaokw ping
   {
	osal_stop_timerEx( GenericApp_TaskID, GENERICAPP_COORDINATER_BROADCAST_PING_EVT );
	CustomApp_coordinater_broadcast_ping();
	osal_start_timerEx( GenericApp_TaskID, GENERICAPP_COORDINATER_BROADCAST_PING_EVT, PING_PERIOD);

	return (events ^ GENERICAPP_COORDINATER_BROADCAST_PING_EVT);	
   }


   if ( events & GENERICAPP_RESET_SB_MSG_EVT )
   {
    	Onboard_soft_reset();
   	return ( events ^ GENERICAPP_RESET_SB_MSG_EVT );
   }

   if ( events & GENERICAPP_LOADSTATUS_MSG_EVT)
   {
       CustomApp_loadstatus_ind();
	return (events ^ GENERICAPP_LOADSTATUS_MSG_EVT);	
   }

   if ( events & GENERICAPP_COORDINATOR_ONLINE_EVT)
   { 
       osal_stop_timerEx( GenericApp_TaskID, GENERICAPP_COORDINATOR_ONLINE_EVT );
       zc_online_report_cnt ++;
       if (zc_online_report_cnt <= 3)
       {
           CustomApp_coordinator_online_req();
	    osal_start_timerEx( GenericApp_TaskID, GENERICAPP_COORDINATOR_ONLINE_EVT, 5000);
       }
	return (events ^ GENERICAPP_COORDINATOR_ONLINE_EVT);	
   }

#if 0
   if ( events & GENERICAPP_ROUTER_ONLINE_EVT)
   {
       osal_stop_timerEx( GenericApp_TaskID, GENERICAPP_ROUTER_ONLINE_EVT );
       zr_online_report_cnt ++;
       if (zr_online_report_cnt <= 3)
       {
           CustomApp_router_online_req();
	    osal_start_timerEx( GenericApp_TaskID, GENERICAPP_ROUTER_ONLINE_EVT, 5000);
       }
	return (events ^ GENERICAPP_ROUTER_ONLINE_EVT);	
   }
#endif   

   if ( events & GENERICAPP_CHANNEL_MSG_EVT )
   {
#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
       CustomApp_WifiChannelReq();
#endif
       return ( events ^ GENERICAPP_CHANNEL_MSG_EVT );
   }

//-----------------------------------------------------chaokw
	if (events & GENERICAPP_PERMIT_JOIN_MSG_EVT)
	{
#ifndef ZDO_COORDINATOR
		if (transfer_join_times++ < 3)
		{
			GenericApp_Device_JoinReq();
			osal_start_timerEx(GenericApp_TaskID,
								GENERICAPP_PERMIT_JOIN_MSG_EVT,
								2000);
		}
#endif
		return ( events ^ GENERICAPP_PERMIT_JOIN_MSG_EVT );
	}

       if (events & GENERICAPP_CLEAR_JOIN_DEVICES_EVT)
	{
		GenericApp_Free_Join_DeviceInfo(zdo_join_device);
		return ( events ^ GENERICAPP_CLEAR_JOIN_DEVICES_EVT );
	}	
//-----------------------------------------------------------
	
  // Discard unknown events
  return 0;
}

/*********************************************************************
 * Event Generation Functions
 */

/*********************************************************************
 * @fn      GenericApp_ProcessZDOMsgs()
 *
 * @brief   Process response messages
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
	ZDO_DeviceAnnce_t *devAnnce=NULL;
	nodeList_t *pnode;
	afAddrType_t addr64;
	afAddrType_t addr16;


  switch ( inMsg->clusterID )
  {
      case Device_annce:    //chaokw
      {
		  devAnnce = (ZDO_DeviceAnnce_t *)osal_mem_alloc( sizeof(  ZDO_DeviceAnnce_t ));
                if ( devAnnce == NULL )
                {
                     return;
                }
		  ZDO_ParseDeviceAnnce( inMsg, devAnnce );
		  if ( devAnnce )
		  {			  
			  addr16.addr.shortAddr = devAnnce->nwkAddr ;	 
			  osal_memcpy(&addr64.addr.extAddr, &devAnnce->extAddr[0], Z_EXTADDR_LEN);
				 
			  pnode=CustomApp_NodeRegister(addr64,addr16);
			  if(pnode!=NULL)
			  {
			       CustomApp_node_register_ind(addr64, addr16);	//0x18
#if defined( LCD_SUPPORTED )			       
				HalLcdWriteStringValue( (char*)"nodregOK:", addr16.addr.shortAddr, 16, HAL_LCD_LINE_2 ); 
#endif
			  }	  
                       osal_mem_free( devAnnce );
		  }
      }
      break;

  
    case End_Device_Bind_rsp:
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
        // Light LED
        HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
      }
#if defined( BLINK_LEDS )
      else
      {
        // Flash LED to show failure
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
      }
#endif
      break;

    case Match_Desc_rsp:
      {
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        if ( pRsp )
        {
          if ( pRsp->status == ZSuccess && pRsp->cnt )
          {
            GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            GenericApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            // Take the first endpoint, Can be changed to search through endpoints
            GenericApp_DstAddr.endPoint = pRsp->epList[0];

            // Light LED
            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
          }
          osal_mem_free( pRsp );
        }
      }
      break;
  }
}

/*********************************************************************
 * @fn      GenericApp_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_4
 *                 HAL_KEY_SW_3
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
static void GenericApp_HandleKeys( uint8 shift, uint8 keys )
{
  zAddrType_t dstAddr;

  // Shift is used to make each button/switch dual purpose.
  if( 0 ) // ( shift )
  {
    if ( keys & HAL_KEY_SW_1 )
    {
    }
    if ( keys & HAL_KEY_SW_2 )
    {
    }
    if ( keys & HAL_KEY_SW_3 )
    {
    }
    if ( keys & HAL_KEY_SW_4 )
    {
    }
  }
  else
  {
    if ( keys & HAL_KEY_SW_1 )
    {
#if defined( SWITCH1_BIND )
      // We can use SW1 to simulate SW2 for devices that only have one switch,
      keys |= HAL_KEY_SW_2;
#elif defined( SWITCH1_MATCH )
      // or use SW1 to simulate SW4 for devices that only have one switch
      keys |= HAL_KEY_SW_4;
#else
      // Normally, SW1 changes the rate that messages are sent
      if ( txMsgDelay > 100 )
      {
        // Cut the message TX delay in half
        txMsgDelay /= 2;
      }
      else
      {
        // Reset to the default
        txMsgDelay = GENERICAPP_SEND_MSG_TIMEOUT;
      }
#endif
    }

    if ( keys & HAL_KEY_SW_2 )
    {
      HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );

      // Initiate an End Device Bind Request for the mandatory endpoint
      dstAddr.addrMode = Addr16Bit;
      dstAddr.addr.shortAddr = 0x0000; // Coordinator
      ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(),
                            GenericApp_epDesc.endPoint,
                            GENERICAPP_PROFID,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            FALSE );
    }

    if ( keys & HAL_KEY_SW_3 )
    {
    }

    if ( keys & HAL_KEY_SW_4 )
    {
      HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
      // Initiate a Match Description Request (Service Discovery)
      dstAddr.addrMode = AddrBroadcast;
      dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
      ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
    }
  }
}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      GenericApp_MessageMSGCB
 *
 * @brief   Data message processor callback.  This function processes
 *          any incoming data - probably from other devices.  So, based
 *          on cluster ID, perform the intended action.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  uint16 msgid;

#if( ZSTACK_DEVICE_BUILD == DEVICE_BUILD_COORDINATOR)		
  nodeList_t *nodeSearch;
  pingpkt_t * pinbgpkt;
  if(pkt->srcAddr.addr.shortAddr != 0x0000)  //chaokw ping
  {
    pinbgpkt = (pingpkt_t *)(pkt->cmd.Data);
    nodeSearch = CustomApp_NodeFindNwkAddrList(pinbgpkt->devAddr16.addr.shortAddr);
    if (nodeSearch!=NULL)
    {	
        nodeSearch->oldtimestamp = nodeSearch->timestamp; 
    }
  }
#endif    

#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)		
  if(pkt->srcAddr.addr.shortAddr == 0x0000)  //chaokw ping
  {
       pingRspTime = osal_getClock();
  }
#endif


  switch ( pkt->clusterId )
  {
    case GENERICAPP_CLUSTERID:
      rxMsgCount += 1;  // Count this message
      HalLedSet ( HAL_LED_4, HAL_LED_MODE_BLINK );  // Blink an LED
#if defined( LCD_SUPPORTED )
      HalLcdWriteString( (char*)pkt->cmd.Data, HAL_LCD_LINE_1 );
      HalLcdWriteStringValue( "Rcvd:", rxMsgCount, 10, HAL_LCD_LINE_2 );
#elif defined( WIN32 )
      WPRINTSTR( pkt->cmd.Data );
#endif
      break;

    case GENERICAPP_DATA_P2P_CLUSTERID:
       CustomApp_AF_P2P_Data_Process(pkt);
	break;

    case GENERICAPP_GROUP_CLUSTERID:
       CustomApp_AF_Group_Data_Process(pkt);	 
	break;

    case GENERICAPP_BROADCAST_CLUSTERID:
        msgid = pkt->cmd.Data[4] | pkt->cmd.Data[5] << 8;
        switch ( msgid )	
        {
          case COORDINATOR_ONLINE_REQ:
             CustomApp_Coordinator_Online();		  	
	      break;
		 
	   default:
             CustomApp_AF_Broadcast_Data_Process(pkt);	   	
             break;
        }
	break;		

    case GENERICAPP_CMD_P2P_CLUSTERID:
#if defined( LCD_SUPPORTED )		
        rxMsgCount += 1;	
        HalLcdWriteStringValue( "cmd Rcvd:", rxMsgCount, 10, HAL_LCD_LINE_2 );
#endif

        msgid = pkt->cmd.Data[0] | pkt->cmd.Data[1] << 8;
        switch ( msgid )
        {
          case PING_REQ_MSG:
             CustomApp_process_ping((pingpkt_t *)(pkt->cmd.Data)); // update node timestamp
	      break;

#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)		
          case ROUTER_ONLINE_REQ:
             CustomApp_Get_Flex_Devinfo();
             break;

          case ROUTER_ONFFLINE_REQ:
             Onboard_soft_reset();
             break;

          case PING_RSP_MSG:   //chaokw ping
             pingRspTime = osal_getClock();
#if defined( LCD_SUPPORTED )		
             HalLcdWriteStringValue( "ping rsp:", pingRspTime, 16, HAL_LCD_LINE_2 );
#endif			 
	      break;
#endif			 
	
	   default:
             break;
         }	
	break;		


	case GENERICAPP_P2P_PERMIT_JOIN_REQ_CLUSTERID:
	  	// gateway coordinator process flex router join from flex request

#if defined( LCD_SUPPORTED )		
        askWhiteListCount += 1;	
        HalLcdWriteStringValue( "ask WL:", askWhiteListCount, 10, HAL_LCD_LINE_2 );
#endif	  	
		GenericApp_Device_JoinRsp(pkt);
		break;

	case GENERICAPP_P2P_PERMIT_JOIN_RSP_CLUSTERID:
	  	// flex router received join permit info from gateway coordinator
		GenericApp_Get_PermitJoin(pkt);
		break;

		
    default:
        break;

  }
}

/*********************************************************************
 * @fn      GenericApp_SendTheMessage
 *
 * @brief   Send "the" message.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_SendTheMessage( void )
{
  char theMessageData[] = "Hello World";

  if ( AF_DataRequest( &GenericApp_DstAddr, &GenericApp_epDesc,
                       GENERICAPP_CLUSTERID,
                       (byte)osal_strlen( theMessageData ) + 1,
                       (byte *)&theMessageData,
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
    // Successfully requested to be sent.
  }
  else
  {
    // Error occurred in request to send.
  }
}

#if defined( IAR_ARMCM3_LM )
/*********************************************************************
 * @fn      GenericApp_ProcessRtosMessage
 *
 * @brief   Receive message from RTOS queue, send response back.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_ProcessRtosMessage( void )
{
  osalQueue_t inMsg;

  if ( osal_queue_receive( OsalQueue, &inMsg, 0 ) == pdPASS )
  {
    uint8 cmndId = inMsg.cmnd;
    uint32 counter = osal_build_uint32( inMsg.cbuf, 4 );

    switch ( cmndId )
    {
      case CMD_INCR:
        counter += 1;  /* Increment the incoming counter */
                       /* Intentionally fall through next case */

      case CMD_ECHO:
      {
        userQueue_t outMsg;

        outMsg.resp = RSP_CODE | cmndId;  /* Response ID */
        osal_buffer_uint32( outMsg.rbuf, counter );    /* Increment counter */
        osal_queue_send( UserQueue1, &outMsg, 0 );  /* Send back to UserTask */
        break;
      }

      default:
        break;  /* Ignore unknown command */
    }
  }
}
#endif



/*********************************************************************
 */

nodeList_t *CustomApp_NodeRegister (afAddrType_t addr64, afAddrType_t addr16)
{
    nodeList_t *nodeSearch;

    nodeSearch = CustomApp_NodeFindMacAddrList(addr64.addr.extAddr);  //chaokw issue

    if (nodeSearch == NULL)
    {
        nodeList_t *node = osal_mem_alloc(sizeof(nodeList_t));  

        if ( node == NULL )
        {
            return NULL;
        }

        node->nextDesc = nodeList;
        nodeList = node;
        node->devAddr64 = addr64;
        node->devAddr16 = addr16;
        node->status= nodeconnected;
        node->sequence = 0;
        node->timestamp = 0;
        node->oldtimestamp =0;
        node->timeoutcnt	=0;

	 return node;
    }

    return NULL;
}


/*********************************************************************
 * @fn      nodeDelete
 *
 * @brief   Delete an Application's EndPoint descriptor and frees the memory
 *
 * @param   EndPoint - Application Endpoint to delete
 *
 * @return  afStatus_SUCCESS - endpoint deleted
 *          afStatus_INVALID_PARAMETER - endpoint not found
 *          afStatus_FAILED - endpoint list empty
 */
nodeStatus_t CustomApp_NodeDelete ( uint16 devaddr )
{
    nodeList_t *nodeCurrent;
    nodeList_t *nodePrevious;

    if (nodeList != NULL)
    {
        nodePrevious = nodeCurrent = nodeList;
        if (nodeCurrent->devAddr16.addr.shortAddr == devaddr )
        {
            nodeList = nodeCurrent->nextDesc;
            osal_mem_free(nodeCurrent);
            return (nodeStatus_SUCCESS);
        }
        else
        {
            for (nodeCurrent = nodePrevious->nextDesc; nodeCurrent != NULL; nodeCurrent = nodeCurrent->nextDesc )  
            {
                if (nodeCurrent->devAddr16.addr.shortAddr== devaddr)
                {
                    nodePrevious->nextDesc = nodeCurrent->nextDesc;
                    osal_mem_free(nodeCurrent);
                    return (nodeStatus_SUCCESS);
                }
                nodePrevious = nodeCurrent;  
            }
        }
        return (nodeStatus_INVALID_PARAMETER);
    }
    else
    {
        return (nodeStatus_FAILED);
    }
}


/*********************************************************************
 * @fn      nodeFinddevaddrList
 *
 * @brief   Find the endpoint description entry from the endpoint
 *          number.
 *
 * @param   EndPoint - Application Endpoint to look for
 *
 * @return  the address to the endpoint/interface description entry
 */
nodeList_t *CustomApp_NodeFindNwkAddrList ( uint16 Addr16 )
{
    nodeList_t *nodeSearch;

    for (nodeSearch = nodeList; (nodeSearch != NULL);
         nodeSearch = nodeSearch->nextDesc)
    {
        if (nodeSearch->devAddr16.addr.shortAddr== Addr16)
        {
            break;
        }
    }
    return nodeSearch;
}


nodeList_t *CustomApp_NodeFindMacAddrList ( uint8* Addr64 )
{
    nodeList_t *nodeSearch;

    for (nodeSearch = nodeList; nodeSearch != NULL;
         nodeSearch = nodeSearch->nextDesc)
    {
        if ( osal_ExtAddrEqual( (uint8 *)(nodeSearch->devAddr64.addr.extAddr),
                                (uint8 *)Addr64 ) == true )
        {
            break;
        }
    }
    return nodeSearch;
}


void CustomApp_ping_request_init( void )
{
	 osal_start_timerEx( GenericApp_TaskID, GENERICAPP_PING_MSG_EVT, PING_PERIOD );
}

void CustomApp_linkstatus_init( void )
{
	 osal_start_timerEx( GenericApp_TaskID, GENERICAPP_LINK_STATUS_EVT, LINKSTAT_PRRIOD );
}


void CustomApp_ping_timeout_check_init( void )   //chaokw ping
{
	 osal_start_timerEx( GenericApp_TaskID, GENERICAPP_PING_TIMEOUT_CHECK_EVT, PING_TIMEOUT_CHECK );
}


void CustomApp_coordinater_broadcast_ping_init( void )   //chaokw ping
{
	 osal_start_timerEx( GenericApp_TaskID, GENERICAPP_COORDINATER_BROADCAST_PING_EVT, PING_PERIOD );
}

void CustomApp_zigbee_start_ind (void)
{

    uint8 msg[13];
    uint8* ExtAddr;	

    msg[0] = MT_CPT_SOP;
    msg[1] = 0x08;
    msg[2] = MT_ZIGBEE_START_IND & 0xff; //0x80
    msg[3] = (MT_ZIGBEE_START_IND  & 0xff00) >> 8;  //0x16

    ExtAddr = NLME_GetExtAddr();
    osal_memcpy(&msg[4], ExtAddr, 8);
#ifdef OPEN_FCS 
    msg[12]=MT_UartCalcFCS (&msg[1], 11);
#else   
    msg[12] = DEFAULTFCS;
#endif

    HalUARTWrite( 0, msg, 13 );
}


void CustomApp_loadstatus_ind(void) 
{
    uint8 msg[15]; 
    uint8* ExtAddr;
    uint16 ShortAddr;	
    
    msg[0] = MT_CPT_SOP;
    msg[1] = 0x0A;  
    msg[2] = MT_LOAD_STATUS_IND & 0xff;   //0x80
    msg[3] = (MT_LOAD_STATUS_IND  & 0xff00) >> 8;  //0x17
    
    ExtAddr = NLME_GetExtAddr();
    osal_memcpy(&msg[4], ExtAddr, 8);

#if( ZSTACK_DEVICE_BUILD == DEVICE_BUILD_COORDINATOR)
    ShortAddr =0x0000;
#else
    ShortAddr = NLME_GetShortAddr();
#endif

    msg[12] = ShortAddr & 0xff;
    msg[13] = (ShortAddr & 0xff00) >> 8;

#ifdef OPEN_FCS 	
    msg[14] = MT_UartCalcFCS(&msg[1], 13);          
#else   
    msg[14] = DEFAULTFCS;
#endif
    HalUARTWrite( 0, msg, 15 );        

}


void CustomApp_node_register_ind(afAddrType_t addr64, afAddrType_t addr16)
{
    uint8 msg[25];
    transferpkt_t *rptmac;
    uint8 fcs;
	
    rptmac = (transferpkt_t *)osal_msg_allocate( sizeof (transferpkt_t) + 8); 
    if (rptmac != NULL)
    {
        rptmac->msgid = REPORT_RPTMAC_MSG; 
        rptmac->msgseq = 0x0001;
        rptmac->srcid = 0x0000;
        osal_memcpy(&rptmac->destid, &addr16.addr.shortAddr, 2);
        rptmac->datalen = 8;

	 rptmac->data = (uint8 *)(rptmac + 1);
	 osal_memcpy(&rptmac->data, &addr64.addr.extAddr, 8);
	 fcs = MT_UartCalcFCS((uint8 *)rptmac, rptmac->datalen + PKT_HEAD_LEN); 
   
        msg[0] = MT_CPT_SOP;
        msg[1] = rptmac->datalen + PKT_HEAD_LEN + 1 ;  //rptmac17 + fcs1 
        msg[2] = MT_NODE_REG_IND & 0xff;
        msg[3] = (MT_NODE_REG_IND & 0xff00) >> 8;

        osal_memcpy(&msg[4], (uint8 *)rptmac, PKT_HEAD_LEN);
        osal_memcpy(&msg[4 + PKT_HEAD_LEN], &rptmac->data, rptmac->datalen);
        msg[4 + rptmac->datalen + PKT_HEAD_LEN] = fcs;

#ifdef OPEN_FCS 			
	 msg[4 + rptmac->datalen + PKT_HEAD_LEN + 1] = MT_UartCalcFCS(&msg[1], rptmac->datalen + PKT_HEAD_LEN + 1 + 3); //rptmac17 + fcs1 + cmd2 + len1
#else  
        msg[4 + rptmac->datalen + PKT_HEAD_LEN + 1] = DEFAULTFCS;
#endif
        HalUARTWrite( 0, msg, rptmac->datalen + PKT_HEAD_LEN + 1 + 3 + 2 ); //0xfe+fcs  2B
	 osal_msg_deallocate((uint8 *) rptmac );
	 
    }

}


#if 0
void CustomApp_node_deregister_ind(afAddrType_t addr64, afAddrType_t addr16)
{
    uint8 msg[25];
    transferpkt_t *rptmac;
    uint8 fcs;
	
    rptmac = (transferpkt_t *)osal_msg_allocate( sizeof (transferpkt_t) +  PKT_HEAD_LEN + 8 + 1); 
    if (rptmac != NULL)
    {
        rptmac->msgid = REPORT_RPTMAC_MSG; 
        rptmac->msgseq = 0x0001;
        rptmac->srcid = 0x0000;
        osal_memcpy(&rptmac->destid, &addr16.addr.shortAddr, 2);
        rptmac->datalen = 8;

	 rptmac->data = (uint8 *)(rptmac + 1);
	 osal_memcpy(&rptmac->data, &addr64.addr.extAddr, 8);
	 fcs = MT_UartCalcFCS((uint8 *)rptmac, rptmac->datalen + PKT_HEAD_LEN); 
   
        msg[0] = MT_CPT_SOP;
        msg[1] = rptmac->datalen + PKT_HEAD_LEN + 1 ;  //rptmac17 + fcs1 
        msg[2] = MT_NODE_DEREG_IND & 0xff;
        msg[3] = (MT_NODE_DEREG_IND & 0xff00) >> 8;

        osal_memcpy(&msg[4], (uint8 *)rptmac, PKT_HEAD_LEN);
        osal_memcpy(&msg[4 + PKT_HEAD_LEN], &rptmac->data, rptmac->datalen);
        msg[4 + rptmac->datalen + PKT_HEAD_LEN] = fcs;

#ifdef OPEN_FCS 			
	 msg[4 + rptmac->datalen + PKT_HEAD_LEN + 1] = MT_UartCalcFCS(&msg[1], rptmac->datalen + PKT_HEAD_LEN + 1 + 3); //rptmac17 + fcs1 + cmd2 + len1
#else  
        msg[4 + rptmac->datalen + PKT_HEAD_LEN + 1] = DEFAULTFCS;
#endif
        HalUARTWrite( 0, msg, rptmac->datalen + PKT_HEAD_LEN + 1 + 3 + 2 ); //0xfe+fcs  2B
	 osal_msg_deallocate((uint8 *) rptmac );
	 
    }

}
#else

void CustomApp_node_deregister_ind(afAddrType_t addr64, afAddrType_t addr16)
{
        uint8 msg[13];
		
        msg[0] = MT_CPT_SOP;
        msg[1] = Z_EXTADDR_LEN ;  
        msg[2] = MT_NODE_DEREG_IND & 0xff;
        msg[3] = (MT_NODE_DEREG_IND & 0xff00) >> 8;
		
        osal_memcpy(&msg[4], &addr64.addr.extAddr, Z_EXTADDR_LEN);
	 msg[4 + Z_EXTADDR_LEN] = MT_UartCalcFCS( &msg[1], MT_UART_HEAD_LEN + Z_EXTADDR_LEN );

        HalUARTWrite( 0, msg, MT_UART_HEAD_LEN + Z_EXTADDR_LEN + 2 );
}

#endif


uint8 cnt=0;
void CustomApp_send_ping_req ( void )  
{
    uint16 parentShortAddr;
    uint8 len=sizeof(pingpkt_t);

#if 0 // chaokw debug
    uint8 send_buf[13];	
    uint32 timestamp;
#endif //================	

    pingpkt_t *pkt = (pingpkt_t *)osal_msg_allocate( sizeof(pingpkt_t) );	
    osal_memset(pkt, 0, sizeof(pingpkt_t));
		
    if(pkt != NULL)
    {
		pkt->msgid = PING_REQ_MSG;
		pkt->devAddr16.addr.shortAddr = NLME_GetShortAddr();
		osal_memcpy(&pkt->devAddr64.addr.extAddr, NLME_GetExtAddr(), Z_EXTADDR_LEN);
		pkt->timestamp = osal_getClock();

		osal_memcpy(&parentShortAddr, &_NIB.nwkCoordAddress, sizeof(uint16));
		pkt->parentaddr = parentShortAddr;

		cnt++;
              pkt->seqnum = cnt;
              if(cnt>254) cnt=2;  //chaokw
       
		if (afStatus_SUCCESS == AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
                       GENERICAPP_CMD_P2P_CLUSTERID,
                       len,   
                       (byte *)pkt,  
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ))   //AF_ACK_REQUEST
              {
#if defined( LCD_SUPPORTED )              
                  HalLcdWriteStringValue( (char*)"pingreqSend", pkt->seqnum, 10, HAL_LCD_LINE_3);
#endif
              }

	       osal_msg_deallocate( (uint8 *)pkt );

    }


#if 0   //chaokw  debug =================================================           
	send_buf[0] = 0xFE;
	send_buf[1 + MT_RPC_POS_LEN] = 0x08;
	send_buf[1 + MT_RPC_POS_CMD0] = 0x0080 & 0xff;
	send_buf[1 + MT_RPC_POS_CMD1] = (0x0080 >> 8) & 0xff;
	
	osal_memcpy( &send_buf[1 + MT_RPC_POS_DAT0], &pingRspTime, 4 );

	timestamp = osal_getClock();
	osal_memcpy( &send_buf[1 + MT_RPC_POS_DAT0 + 4], &timestamp, 4 );


	send_buf[1 + MT_RPC_POS_DAT0 + SADDR_EXT_LEN] =
	  MT_UartCalcFCS( &send_buf[1 + MT_RPC_POS_LEN], MT_UART_HEAD_LEN + SADDR_EXT_LEN );

	HalUARTWrite( 0, send_buf, MT_UART_HEAD_LEN + SADDR_EXT_LEN + 2 );
#endif //======================================================	
	
    return;
}



void CustomApp_send_ping_rsp ( uint16 Addr16 )  
{
    uint16 msgid = PING_RSP_MSG;;

    SerialApp_TxAddr_p2p.addr.shortAddr = Addr16;
    if (afStatus_SUCCESS == AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
                   GENERICAPP_CMD_P2P_CLUSTERID,
                   2,   
                   (byte *)&msgid,  
                   &GenericApp_TransID,
                   AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ))
    {
#if defined( LCD_SUPPORTED )              
     HalLcdWriteStringValue( (char*)"pingrspSend", 1, 10, HAL_LCD_LINE_3);
#endif
    }
    return;
}

void CustomApp_coordinater_broadcast_ping( void )  //chaokw ping
{
    uint16 msgid = PING_RSP_MSG;;

    SerialApp_TxAddr_broadcast.addr.shortAddr = 0xffff;
    if (afStatus_SUCCESS == AF_DataRequest( &SerialApp_TxAddr_broadcast, &GenericApp_epDesc,
                   GENERICAPP_CMD_P2P_CLUSTERID,
                   2,   
                   (byte *)&msgid,  
                   &GenericApp_TransID,
                   AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ))
    {
#if defined( LCD_SUPPORTED )              
     HalLcdWriteStringValue( (char*)"broadcast ping", 1, 10, HAL_LCD_LINE_3);
#endif
    }
    return;

}

void CustomApp_linkstatus_updated ( void )
{
    nodeList_t *nodeSearch = NULL;
    uint8 status;
//#if defined( LCD_SUPPORTED )    	
    uint8 activeCount;
//#endif


node_check:
    for (nodeSearch = nodeList; nodeSearch != NULL; nodeSearch = nodeSearch->nextDesc)
    {
        if (nodeSearch->status == nodedisconnected)
        {
            //node delete
	     status = CustomApp_NodeDelete( nodeSearch->devAddr16.addr.shortAddr );
            if(nodeStatus_SUCCESS == status)
            {
                  CustomApp_node_deregister_ind(nodeSearch->devAddr64, nodeSearch->devAddr16);
                  //CustomApp_router_offline_req ( nodeSearch->devAddr16 );		//chaokw ping
#if defined( LCD_SUPPORTED )				  
                  HalLcdWriteStringValue( (char*)"nodedelete OK:", status, 16, HAL_LCD_LINE_3 );
#endif
            }
            else
            {
#if defined( LCD_SUPPORTED )            
                  HalLcdWriteStringValue( (char*)"nodedelete NOK:", status, 16, HAL_LCD_LINE_3 );
#endif
            }
            goto node_check;   //break;
        }
        else if (nodeSearch->status == nodeconnected)
        {		
             if(nodeSearch->timestamp == nodeSearch->oldtimestamp)
             {
                 nodeSearch->timeoutcnt ++;
	          if(nodeSearch->timeoutcnt < TIMEOUT_CNT)
                 {
                         break;
                 }
                 nodeSearch->timeoutcnt = 0;
                 nodeSearch->status = nodedisconnected;
             }
        }
        nodeSearch->oldtimestamp = nodeSearch->timestamp;		
    }

    activeCount = CustomApp_get_connected_node_count();
#if defined( LCD_SUPPORTED )    	
    HalLcdWriteStringValue( (char*)"connected:", activeCount, 10, HAL_LCD_LINE_1 );
#endif


#if 1
    //exception: panid conflict 
    if(activeCount > Authlist_item_cnt())
    {
#if defined ( NV_RESTORE )		
        NLME_InitNV();
        NLME_SetDefaultNV();
        ZDSecMgrClearNVKeyValues();
#endif	  
        Onboard_soft_reset();
    }
#endif


    return;
}


void  CustomApp_process_ping( pingpkt_t *pkt )
{
        nodeList_t *nodeSearch;
	 nodeList_t *pnode;
        nodeSearch = CustomApp_NodeFindNwkAddrList(pkt->devAddr16.addr.shortAddr);
        if (nodeSearch!=NULL)
        {
           if(nodeSearch->status == nodeconnected)
           {
		   nodeSearch->timestamp = pkt->timestamp;
		   if (pkt->seqnum <= 2)
		   CustomApp_node_register_ind(pkt->devAddr64, pkt->devAddr16);
           }
        }
	 else
	 {
            pnode=CustomApp_NodeRegister(pkt->devAddr64, pkt->devAddr16);
            if(pnode!=NULL)
            {
                 CustomApp_node_register_ind(pkt->devAddr64, pkt->devAddr16);	//0x18
                 CustomApp_router_online_req(pkt->devAddr16);
#if 0				 
                 osal_start_timerEx( GenericApp_TaskID,
					GENERICAPP_ROUTER_ONLINE_EVT,
					5000); 		
#endif
                 CustomApp_router_online_req(pkt->devAddr16);
#if defined( LCD_SUPPORTED )
                 HalLcdWriteStringValue( (char*)"nodregOK:", pkt->devAddr16.addr.shortAddr, 16, HAL_LCD_LINE_2 );  
#endif
            }	  
	 }
	 nodeSearch->oldtimestamp = nodeSearch->timestamp;  //chaokw ping
	 CustomApp_send_ping_rsp(pkt->devAddr16.addr.shortAddr);  //chaokw ping
	 
}


#if( ZSTACK_DEVICE_BUILD != DEVICE_BUILD_COORDINATOR)	
void CustomApp_ping_timeout_check ( void )
{
    uint32 timestamp = osal_getClock();
#if 0	
#if defined( LCD_SUPPORTED )
    HalLcdWriteStringValue( (char*)"ping timeout:", (timestamp - pingRspTime), 16, HAL_LCD_LINE_3 );  
#endif	
#endif
    if ( abs(timestamp - pingRspTime) >= 60 ) 
        Onboard_soft_reset();	
}
#endif

uint8 CustomApp_get_connected_node_count( void )
{
	uint8 activeCount = 0;
	nodeList_t *nodeSearch;

       for (nodeSearch = nodeList; (nodeSearch != NULL); nodeSearch = nodeSearch->nextDesc)
       {
        if (nodeSearch->status == nodeconnected)
        {
            activeCount ++;
        }
       }
       return activeCount;
}


bool GenericApp_Device_JoinRsp(afIncomingMSGPacket_t *pkt)
{
	transferpkt_t *protocol_pack;
	uint16 clusterid;
	uint8 buf[100];
	bool ret = FALSE;
	uint8 extaddr[SADDR_EXT_LEN] = {0};

	osal_memcpy(extaddr, pkt->cmd.Data + PKT_HEAD_LEN , SADDR_EXT_LEN);

	protocol_pack = (transferpkt_t *)osal_msg_allocate( sizeof( transferpkt_t ) + SADDR_EXT_LEN + 1);
	protocol_pack->msgid = PROT_JOIN_RSP_MSG;
	protocol_pack->msgseq = 0x0001;
	protocol_pack->destid = (pkt->cmd.Data[7] << 8) | pkt->cmd.Data[6];
	protocol_pack->srcid = NLME_GetShortAddr();
	protocol_pack->datalen = SADDR_EXT_LEN;

	if (!Authlist_verify_device(0x0000, extaddr, 0, 0))
          osal_memset(extaddr, 0, SADDR_EXT_LEN);

	protocol_pack->data = (uint8 *)(protocol_pack + 1);
	osal_memcpy(protocol_pack->data, extaddr, SADDR_EXT_LEN);
	osal_memcpy( buf, (uint8 *)protocol_pack, PKT_HEAD_LEN );
	osal_memcpy(&buf[PKT_HEAD_LEN], protocol_pack->data, protocol_pack->datalen);
	protocol_pack->fcs = MT_UartCalcFCS( buf, PKT_HEAD_LEN + protocol_pack->datalen );
	buf[PKT_HEAD_LEN + protocol_pack->datalen] = protocol_pack->fcs;
	clusterid = GENERICAPP_P2P_PERMIT_JOIN_RSP_CLUSTERID;
	SerialApp_TxAddr_p2p.addr.shortAddr = protocol_pack->destid;

	if ( AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
						clusterid,
						PKT_HEAD_LEN + SADDR_EXT_LEN + 1,
						buf, //(uint8 *)protocol_pack,
						&GenericApp_TransID,
						AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
	{
		ret = TRUE;
	}
	else
	{
		ret = FALSE;
	}
	osal_msg_deallocate( (uint8 *)protocol_pack );
	return ret;
}


void GenericApp_Get_PermitJoin(afIncomingMSGPacket_t *pkt)
{
	uint8 extaddr[SADDR_EXT_LEN];
	ZDO_Permit_Join_Device_t *tmp_join_device = NULL;
	ZDO_Permit_Join_Device_t *new_join_device = NULL;
	uint8 device_cnt = 0;
	static uint8 device_index = 0;

	osal_memcpy(extaddr, pkt->cmd.Data + PKT_HEAD_LEN, SADDR_EXT_LEN);
 
	tmp_join_device = zdo_join_device;
	while (tmp_join_device)
	{
		if (osal_memcmp(tmp_join_device->permit_device.extaddr, extaddr, SADDR_EXT_LEN))
		{
			return;
		}
		tmp_join_device = tmp_join_device->next;
	}
	// Calculate permit device count
	tmp_join_device = zdo_join_device;
	if (tmp_join_device)
	{
		device_cnt = 1;
		while (tmp_join_device->next)
		{
			device_cnt++;
			tmp_join_device = tmp_join_device->next;
		}
	}
	if (device_cnt < MAX_JOIN_DEVICE_CNT)
	{
		new_join_device = (ZDO_Permit_Join_Device_t *) osal_mem_alloc(sizeof(ZDO_Permit_Join_Device_t));
		osal_memcpy(new_join_device->permit_device.extaddr, extaddr, SADDR_EXT_LEN);
		new_join_device->next = NULL;
		if (tmp_join_device)
		{
			tmp_join_device->next = new_join_device;
		}
		else
		{
			zdo_join_device = new_join_device;
		}
		device_cnt++;
	}
	else
	{
		tmp_join_device = zdo_join_device;
		for (uint8 i = 0; i < device_index; i++)
		{
			tmp_join_device = tmp_join_device->next;
		}
		osal_memcpy(tmp_join_device->permit_device.extaddr, extaddr, SADDR_EXT_LEN);
		if (++device_index >= MAX_JOIN_DEVICE_CNT)
		{
			device_index = 0;
		}
	}
	
}

bool GenericApp_Device_JoinReq(void)
{
	transferpkt_t *protocol_pack;
	uint16 clusterid;
	uint8 buf[200];
	bool ret = FALSE;

	protocol_pack = (transferpkt_t *)osal_msg_allocate( sizeof( transferpkt_t ) + SADDR_EXT_LEN + 1);
	if ( protocol_pack == NULL )
	{
		return ZFailure;
	}

	protocol_pack->msgid = PROT_JOIN_REQ_MSG;
	protocol_pack->msgseq = 0x0001;
	protocol_pack->destid = 0x0000;
	protocol_pack->srcid = NLME_GetShortAddr();
	protocol_pack->datalen = SADDR_EXT_LEN;

	protocol_pack->data = (uint8 *)(protocol_pack + 1);
	osal_memcpy(protocol_pack->data, join_device_extaddr, SADDR_EXT_LEN);
	//+msg id 2B+msg_seq2B+len byte+ dest_id 2B+src_id 2B
	osal_memcpy( buf, (uint8 *)protocol_pack, PKT_HEAD_LEN );
	osal_memcpy(&buf[PKT_HEAD_LEN], protocol_pack->data, SADDR_EXT_LEN);
	protocol_pack->fcs = MT_UartCalcFCS( buf, PKT_HEAD_LEN + protocol_pack->datalen );
	buf[PKT_HEAD_LEN + protocol_pack->datalen] = protocol_pack->fcs;

	clusterid = GENERICAPP_P2P_PERMIT_JOIN_REQ_CLUSTERID;
	SerialApp_TxAddr_p2p.addr.shortAddr = 0x0000;

	if ( AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
						clusterid,
						PKT_HEAD_LEN + SADDR_EXT_LEN + 1,
						buf, //(uint8 *)protocol_pack,
						&GenericApp_TransID,
						AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
	{
		ret = TRUE;
	}
	else
	{
		ret = FALSE;
	}
	osal_msg_deallocate( (uint8 *)protocol_pack );
	return ret;
}

void GenericApp_Free_Join_DeviceInfo(ZDO_Permit_Join_Device_t *permit_join_device)  
{
	ZDO_Permit_Join_Device_t *head_join_device;
	ZDO_Permit_Join_Device_t *tmp_join_device = permit_join_device;
	while (tmp_join_device)
	{
		head_join_device = tmp_join_device->next;
		osal_mem_free(tmp_join_device);   //chaokw open issue
		tmp_join_device = head_join_device;
	}
}
