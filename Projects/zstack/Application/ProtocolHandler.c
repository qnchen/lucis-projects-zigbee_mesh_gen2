/**************************************************************************************************
  Filename:       ProtocolHandler.c
  Revised:        2017-03-19

  Description -   Serial Transfer Application node authentication related func.

**************************************************************************************************/


/*********************************************************************
 * INCLUDES
 */
#include "MT_UART.h"
#include "MT_RPC.h"
#include "ZComDef.h"
#include "ZDApp.h"
#include "hal_uart.h"
#include "GenericApp.h"
#include "hal_aes.h"
#include "hal_ccm.h"
#include "osal_nv.h"
#include "mac_rx_onoff.h"
#include "mac_radio.h"
#include "mac_mcu.h"
#include "mac_low_level.h"
#include "string.h"

#include "GenericApp.h"
#include "ProtocolHandler.h"
#include "Aps_groups.h"


/*********************************************************************
 * LOCAL VARIABLES
 */
#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
// For zigbee channel select
bool __xdata getWifiChannel = FALSE;
bool __xdata zgChannelUpdate = FALSE;
// For wifi channel, every bit indicate one channel
uint16 __xdata wifiChannel = 0x0000;

extern uint32 __xdata zgDefaultChannelMask;
#endif

PROT_GROUP_STRUCT Prot_Group_T;
aps_Group_t GenericApp_Group;
#define GENERICAPP_GROUP 0x0001

uint8 *uart_buf;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void CustomApp_Uart_Send( uint8 port, uint16 cmd, uint8 *pbuf, uint8 len, uint8 offset );
#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
static void CustomApp_ZigbeeChannelMask( uint16 wifiChannel );
#endif


/*********************************************************************
 * FUNCTIONS
 */


uint8 CustomApp_Send_P2P_Data( afIncomingMSGPacket_t *pkt )
{
    uint8 *msg;	
    uint8 ret = ZFailure;
	
    msg = (uint8 *)osal_msg_allocate( pkt->cmd.DataLength);
    if ( msg == NULL )
    {
        return ret;
    }

    osal_memcpy(msg, pkt->cmd.Data, pkt->cmd.DataLength);
    SerialApp_TxAddr_p2p.addr.shortAddr = (pkt->cmd.Data[5 + MT_UART_HEAD_LEN] << 8) | pkt->cmd.Data[4 + MT_UART_HEAD_LEN];
    if ( AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
                         GENERICAPP_DATA_P2P_CLUSTERID,
                         (byte) (pkt->cmd.DataLength) ,
                         (byte *)(msg),
                         &GenericApp_TransID,
                         AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
    {
        ret = ZSuccess;
    }
    osal_msg_deallocate( msg );
    return ret;	
}


uint8 CustomApp_Send_BroadCast_Data( afIncomingMSGPacket_t *pkt )
{
	uint8 *msg;
	uint8 ret = ZFailure;

	msg = (uint8 *)osal_msg_allocate( pkt->cmd.DataLength);
	if ( msg == NULL )
	{
		return ret;
	}

	osal_memcpy(msg, pkt->cmd.Data, pkt->cmd.DataLength);
	SerialApp_TxAddr_broadcast.addr.shortAddr = 0xffff;

	if ( AF_DataRequest( &SerialApp_TxAddr_broadcast, &GenericApp_epDesc,
						 GENERICAPP_BROADCAST_CLUSTERID,
						 (byte) (pkt->cmd.DataLength) ,
						 (byte *)(msg),
						 &GenericApp_TransID,
						 AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
	{
		ret = ZSuccess;
	}
       osal_msg_deallocate( msg );
	return ret;
}


uint8 CustomApp_Send_Group_Data( afIncomingMSGPacket_t *pkt )  //chaokw
{
    uint8 msg[20];
    uint8 ret = ZFailure;	
    uint8 fcs;	
	
    transferpkt_t *lightstate = (transferpkt_t *)osal_msg_allocate(sizeof (transferpkt_t) +  2);
    if (lightstate != NULL)
    {
        lightstate->msgid = GROUP_LIGHT_STATE_MSG; 
        lightstate->msgseq = 0x0001;
        lightstate->srcid = NLME_GetShortAddr();
        lightstate->destid = Prot_Group_T.group_id;
        lightstate->datalen = 2;

        lightstate->data = (uint8 *)(lightstate + 1);
        osal_memcpy(&lightstate->data, &pkt->cmd.Data[3], 2);
        fcs = MT_UartCalcFCS((uint8 *)lightstate, lightstate->datalen + PKT_HEAD_LEN); 

        msg[0] = MT_CPT_SOP;
        msg[1] = lightstate->datalen + PKT_HEAD_LEN + 1;
        msg[2] = MT_DATA_P2P_RSP & 0xff;   
        msg[3] = (MT_DATA_P2P_RSP  & 0xff00) >> 8;

        osal_memcpy(&msg[4], (uint8 *)lightstate, PKT_HEAD_LEN);
        osal_memcpy(&msg[4 + PKT_HEAD_LEN], &lightstate->data, lightstate->datalen);
        msg[4 + lightstate->datalen + PKT_HEAD_LEN] = fcs;

#ifdef OPEN_FCS 			
        msg[4 + lightstate->datalen + PKT_HEAD_LEN + 1] = MT_UartCalcFCS(&msg[1], lightstate->datalen + PKT_HEAD_LEN + 1 + 3); //rptmac17 + fcs1 + cmd2 + len1
#else  
        msg[4 + lightstate->datalen + PKT_HEAD_LEN + 1] = DEFAULTFCS;
#endif

        if ( AF_DataRequest( &SerialApp_TxAddr_group, &GenericApp_epDesc,
                             GENERICAPP_GROUP_CLUSTERID,
                             (byte) (lightstate->datalen + PKT_HEAD_LEN + 1 + 3 + 2) ,
                             (byte *)(msg),
                             &GenericApp_TransID,
                             AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
        {
             ret = ZSuccess;
        }

        osal_msg_deallocate( (uint8 *)lightstate );
    }
	
    return ret;	
}


void CustomApp_Get_ExtAddr( void )
{
	uint8 send_buf[13];
	uint16 cmd;

	cmd = MT_MAC_RSP_MSG;
	send_buf[0] = 0xFE;
	send_buf[1 + MT_RPC_POS_LEN] = SADDR_EXT_LEN;
	send_buf[1 + MT_RPC_POS_CMD0] = cmd & 0xff;
	send_buf[1 + MT_RPC_POS_CMD1] = (cmd >> 8) & 0xff;
	osal_memcpy( &send_buf[1 + MT_RPC_POS_DAT0], NLME_GetExtAddr(), SADDR_EXT_LEN );
	send_buf[1 + MT_RPC_POS_DAT0 + SADDR_EXT_LEN] =
	  MT_UartCalcFCS( &send_buf[1 + MT_RPC_POS_LEN], MT_UART_HEAD_LEN + SADDR_EXT_LEN );

	HalUARTWrite( 0, send_buf, MT_UART_HEAD_LEN + SADDR_EXT_LEN + 2 );
}


void CustomApp_Get_NWKInfo( void )  //chaokw
{
	uint8 send_buf[13];
	uint16 cmd;

	cmd = MT_NWKINFO_RSP_MSG;
	send_buf[0] = 0xFE;
	send_buf[1 + MT_RPC_POS_LEN] = 0x03;
	send_buf[1 + MT_RPC_POS_CMD0] = cmd & 0xff;
	send_buf[1 + MT_RPC_POS_CMD1] = (cmd >> 8) & 0xff;
	
       osal_memcpy(&send_buf[1 + MT_RPC_POS_DAT0], &_NIB.nwkPanId, 2);   
       send_buf[1 + MT_RPC_POS_DAT0 + 2] = macPhyChannel;
	
	send_buf[1 + MT_RPC_POS_DAT0 + 3] =
	  MT_UartCalcFCS( &send_buf[1 + MT_RPC_POS_LEN], MT_UART_HEAD_LEN + 3 );

	HalUARTWrite( 0, send_buf, MT_UART_HEAD_LEN + 3 + 2 );
}


void CustomApp_Get_Active_Cnt( void )
{
       uint8 activeCount = 0;
       activeCount = CustomApp_get_connected_node_count();
	CustomApp_Uart_Send( 0, (uint16)MT_ACTIVE_CNT_RSP_MSG, (uint8 *)&activeCount, 1 , 0 );
}


void CustomApp_Get_Version( void )
{
	CustomApp_Uart_Send( 0, (uint16)MT_FW_VERSION_RSP_MSG, (uint8 *)fw_version, (uint8)ZIGBEE_FW_VERSION_LEN , 0 );
}


void CustomApp_FW_Update( void )
{
	CustomApp_Uart_Send( 0, (uint16)MT_FW_UPDATE_RSP_MSG, NULL, 0 , 0 );
	osal_start_timerEx( GenericApp_TaskID, GENERICAPP_RESET_SB_MSG_EVT, 500);
}

static void CustomApp_Uart_Send( uint8 port, uint16 cmd, uint8 *pbuf, uint8 len, uint8 offset )
{
	uint8 send_buf[256];

	send_buf[0] = 0xFE;
	send_buf[1 + MT_RPC_POS_LEN] = len;
	send_buf[1 + MT_RPC_POS_CMD0] = cmd & 0xff;
	send_buf[1 + MT_RPC_POS_CMD1] = (cmd >> 8) & 0xff;
	if ( len )
	{
		osal_memcpy( &send_buf[1 + MT_RPC_POS_DAT0], &pbuf[offset], len );
	}
	send_buf[1 + MT_RPC_POS_DAT0 + len] =
		MT_UartCalcFCS( &send_buf[1 + MT_RPC_POS_LEN], MT_UART_HEAD_LEN + len );
	HalUARTWrite( port, send_buf, MT_UART_HEAD_LEN + len + 2 );
}


void CustomApp_AF_P2P_Data_Process(afIncomingMSGPacket_t *pkt)
{
    // | SOP | Data Length |   CMD0   |CMD1   |   Data   |  FCS  |
    // | 1     |  1                |     1        |    1     |  0-Len   |   1   |  
    uart_buf = (uint8 *)osal_msg_allocate(pkt->cmd.DataLength + 2);
    if ( uart_buf == NULL )
    {
        return;
    } 		
    uart_buf[0] = MT_UART_SOF;	      
    osal_memcpy(&uart_buf[1], pkt->cmd.Data, pkt->cmd.DataLength);
    uart_buf[pkt->cmd.DataLength + 1]  = MT_UartCalcFCS(&uart_buf[1], pkt->cmd.DataLength);              
    HalUARTWrite(0, uart_buf, pkt->cmd.DataLength + 2);   
	
    osal_msg_deallocate ((uint8 *)uart_buf); 
}

void CustomApp_AF_Broadcast_Data_Process(afIncomingMSGPacket_t *pkt)
{
    CustomApp_AF_P2P_Data_Process(pkt);
}

void CustomApp_AF_Group_Data_Process(afIncomingMSGPacket_t *pkt)
{
    uart_buf = (uint8 *)osal_msg_allocate(pkt->cmd.DataLength);
    if ( uart_buf == NULL )
    {
        return;
    } 		
    osal_memcpy(uart_buf, pkt->cmd.Data, pkt->cmd.DataLength);
    HalUARTWrite(0, uart_buf, pkt->cmd.DataLength);   
    osal_msg_deallocate ((uint8 *)uart_buf); 
}


#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
void CustomApp_WifiChannelReq( void )
{
	uint16 cmd;
	uint8 buf[6] = {0};

	if ( !getWifiChannel )
	{
		cmd = MT_WIFI_CHANNEL_REQ;
		buf[0] = MT_UART_SOF;
		buf[1 + MT_RPC_POS_LEN] = 0x00;
		buf[1 + MT_RPC_POS_CMD0] = cmd & 0xFF;
		buf[1 + MT_RPC_POS_CMD1] = (cmd >> 8) & 0xFF;
		buf[1 + MT_RPC_POS_DAT0] = MT_UartCalcFCS( &buf[1], MT_UART_HEAD_LEN);
		HalUARTWrite( 0, buf, MT_RPC_POS_DAT0 + 2 );
		osal_start_timerEx( GenericApp_TaskID,
						GENERICAPP_CHANNEL_MSG_EVT,
						5000 );
	}
}

void CustomApp_ChannelSelect(afIncomingMSGPacket_t *MSGpkt)
{
	uint16 wifi_channel;
	wifi_channel = (uint16)(*(MSGpkt->cmd.Data));
	if ( (wifi_channel < 1) || (wifi_channel > 13) )
	{
		HalUARTWrite( 0, "wifi channel error", 19 );
		return;
	}
	CustomApp_ZigbeeChannelMask( wifi_channel );
	if ( zgChannelUpdate )
	{
		HAL_SYSTEM_RESET();
	}
	getWifiChannel = TRUE;
}

static void CustomApp_ZigbeeChannelMask( uint16 wifiChannel )
{
	// CH1 frequency  - 11
	uint32 wifiChannelMin = 2401;
	// CH13 frequency + 11
	uint32 wifiChannleMax = 2495;
	uint32 wifiFrequency;
	uint32 zgFrequency;
	uint32 zgChannelMask;
	uint8 tryCount;
	// The BW of wifi is 22MHz
	wifiFrequency = 2412 + 6 * (wifiChannel - 1);
	if ( wifiFrequency -11 > wifiChannelMin )
	{
		wifiChannelMin = wifiFrequency -11;
	}
	if ( wifiFrequency + 11 < wifiChannleMax )
	{
		wifiChannleMax = wifiFrequency + 11;
	}
	// Calculate for the zigbee frequency in the gateway wifi
	zgChannelMask = 0;
	for ( uint8 i = 11; i <= 26; i++ )
	{
		zgFrequency = 2405 + 5 * (i - 11);
		if ( (zgFrequency >= wifiChannelMin) && (zgFrequency <= wifiChannleMax) )
		{
			zgChannelMask |= ((uint32)1 << i);
			if ( i == macPhyChannel )
			{
				zgChannelUpdate = TRUE;
			}
		}
	}
	zgChannelMask = MAX_CHANNELS_24GHZ & (~zgChannelMask);
	if ( zgChannelMask != zgDefaultChannelMask )
	{
		zgDefaultChannelMask = zgChannelMask;
	}
	macRxOff();
	tryCount = 3;
	while ( tryCount --)
	{
		if ( osal_nv_write( ZCD_NV_CHANNEL_MASK,
				  0,
				  sizeof( zgDefaultChannelMask ),
				  &zgDefaultChannelMask ) != SUCCESS )
		{
			HalUARTWrite( 0, "Write ZCD_NV_CHANNEL_MASK failed !", 35 );
		}
	}
	macRxOn();
}
#endif


void CustomApp_Set_Multiway(afIncomingMSGPacket_t *pkt)
{
    /*
    B1-B0	  B3-B2  B5-B4  B7-B6	 B8    B10-B9			B11		B12			B13			B14				B14
                                                        GROUP ID	GROUP Type	Mem Struct	Master loc	GROUP Master      CSUM
    */      
    uint8 msg[20];
    uint8 len;
    // nv restore and set mutliway
    Prot_Group_T.group_type = pkt->cmd.Data[2];
    Prot_Group_T.mem_struct = pkt->cmd.Data[3];
    Prot_Group_T.master_location = pkt->cmd.Data[4];
    Prot_Group_T.group_master = pkt->cmd.Data[5];
    if(Prot_Group_T.group_type == 1)//add to group
    {
        Prot_Group_T.group_id = pkt->cmd.Data[0] | (pkt->cmd.Data[1] << 8);
        Register_Group_Multiway();
    }
    else if(Prot_Group_T.group_type == 2)//remove from group
    {
        aps_RemoveGroup( GENERICAPP_ENDPOINT, GenericApp_Group.ID);
    }
    Nvram_Write_Multiway();

    osal_nv_read(ZCD_NV_MULTIWAY, 0, sizeof(PROT_GROUP_STRUCT), &Prot_Group_T);

    //send multiway resp to flex mcu
    len = sizeof(Prot_Group_T);
    msg[0] = MT_CPT_SOP;
    msg[1] = len;	
    msg[2] = MT_MULTIWAY_SET_RSP_MSG & 0xff;
    msg[3] = (MT_MULTIWAY_SET_RSP_MSG & 0xff00) >> 8;
    osal_memcpy(&msg[4], &Prot_Group_T, len);

    msg[4] = Prot_Group_T.group_id & 0xff;
    msg[5] = (Prot_Group_T.group_id & 0xff00) >> 8;

#ifdef OPEN_FCS 			
    msg[4 + len] = MT_UartCalcFCS(&msg[1], len + 3); 
#else  
    msg[4 + len] = DEFAULTFCS;
#endif
    HalUARTWrite( 0, msg, len + 5 );
}


uint8 CustomApp_coordinator_online_req ( void )
{
    uint8 msg[20];
    uint8 ret = ZFailure;	
    uint8 data = 0xff;
    uint8 fcs;	
	
    transferpkt_t *onlinestate = (transferpkt_t *)osal_msg_allocate(sizeof (transferpkt_t) +  1);   // len=1
    if (onlinestate != NULL)
    {
        onlinestate->msgid = COORDINATOR_ONLINE_REQ; 
        onlinestate->msgseq = 0x0001;
        onlinestate->srcid = NLME_GetShortAddr();
        onlinestate->destid = 0xffff;
        onlinestate->datalen = 1;

        onlinestate->data = (uint8 *)(onlinestate + 1);
        osal_memcpy(&onlinestate->data, &data, 1);
        fcs = MT_UartCalcFCS((uint8 *)onlinestate, onlinestate->datalen + PKT_HEAD_LEN); 

        msg[0] = MT_CPT_SOP;
        msg[1] = onlinestate->datalen + PKT_HEAD_LEN + 1;
        msg[2] = MT_BROADCAST_REQ & 0xff;   
        msg[3] = (MT_BROADCAST_REQ  & 0xff00) >> 8;

        osal_memcpy(&msg[4], (uint8 *)onlinestate, PKT_HEAD_LEN);
        osal_memcpy(&msg[4 + PKT_HEAD_LEN], &onlinestate->data, onlinestate->datalen);
        msg[4 + onlinestate->datalen + PKT_HEAD_LEN] = fcs;
		
#ifdef OPEN_FCS 			
        msg[4 + onlinestate->datalen + PKT_HEAD_LEN + 1] = MT_UartCalcFCS(&msg[1], onlinestate->datalen + PKT_HEAD_LEN + 1 + 3); //rptmac17 + fcs1 + cmd2 + len1
#else  
        msg[4 + onlinestate->datalen + PKT_HEAD_LEN + 1] = DEFAULTFCS;
#endif

        if ( AF_DataRequest( &SerialApp_TxAddr_broadcast, &GenericApp_epDesc,
                             GENERICAPP_BROADCAST_CLUSTERID,
                             (byte) (onlinestate->datalen + PKT_HEAD_LEN + 1 + 3 + 2) ,
                             (byte *)(msg),
                             &GenericApp_TransID,
                             AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
        {
             ret = ZSuccess;
        }

        osal_msg_deallocate( (uint8 *)onlinestate );
    }
    return ret;	
}



uint8 CustomApp_router_online_req ( afAddrType_t addr16 )
{
    uint8 ret = ZFailure;	
    uint8 data = 0xff;
	
    transferpkt_t *onlinestate = (transferpkt_t *)osal_msg_allocate(sizeof (transferpkt_t) +  1);  
    {
        onlinestate->msgid = ROUTER_ONLINE_REQ; 
        onlinestate->msgseq = 0x0001;
        onlinestate->srcid = NLME_GetShortAddr();
        onlinestate->destid = addr16.addr.shortAddr;
        onlinestate->datalen = 1;

        onlinestate->data = (uint8 *)(onlinestate + 1);
        osal_memcpy(&onlinestate->data, &data, 1);
        onlinestate->fcs = MT_UartCalcFCS((uint8 *)onlinestate, onlinestate->datalen + PKT_HEAD_LEN); 

        SerialApp_TxAddr_p2p.addr.shortAddr = addr16.addr.shortAddr;
        if ( AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
                             GENERICAPP_CMD_P2P_CLUSTERID,
                             (byte) (onlinestate->datalen + PKT_HEAD_LEN + 1) ,
                             (byte *)(onlinestate),
                             &GenericApp_TransID,
                             AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
        {
             ret = ZSuccess;
        }
        osal_msg_deallocate( (uint8 *)onlinestate );
    }
    return ret;	
}



uint8 CustomApp_router_offline_req ( afAddrType_t addr16 )
{
    uint8 ret = ZFailure;	
    uint8 data = 0xff;
	
    transferpkt_t *onlinestate = (transferpkt_t *)osal_msg_allocate(sizeof (transferpkt_t) +  1);  
    {
        onlinestate->msgid = ROUTER_ONFFLINE_REQ; 
        onlinestate->msgseq = 0x0001;
        onlinestate->srcid = NLME_GetShortAddr();
        onlinestate->destid = addr16.addr.shortAddr;
        onlinestate->datalen = 1;

        onlinestate->data = (uint8 *)(onlinestate + 1);
        osal_memcpy(&onlinestate->data, &data, 1);
        onlinestate->fcs = MT_UartCalcFCS((uint8 *)onlinestate, onlinestate->datalen + PKT_HEAD_LEN); 

        SerialApp_TxAddr_p2p.addr.shortAddr = addr16.addr.shortAddr;
        if ( AF_DataRequest( &SerialApp_TxAddr_p2p, &GenericApp_epDesc,
                             GENERICAPP_CMD_P2P_CLUSTERID,
                             (byte) (onlinestate->datalen + PKT_HEAD_LEN + 1) ,
                             (byte *)(onlinestate),
                             &GenericApp_TransID,
                             AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
        {
             ret = ZSuccess;
        }
        osal_msg_deallocate( (uint8 *)onlinestate );
    }
    return ret;	
}



void CustomApp_Coordinator_Online( void )
{
	uint8 send_buf[13];
	uint16 cmd;
	
	cmd = MT_COORDINATOR_ONLINE_REQ_MSG;
	send_buf[0] = 0xFE;
	send_buf[1 + MT_RPC_POS_LEN] = 0x01;
	send_buf[1 + MT_RPC_POS_CMD0] = cmd & 0xff;
	send_buf[1 + MT_RPC_POS_CMD1] = (cmd >> 8) & 0xff;
	send_buf[1 + MT_RPC_POS_DAT0] = 0xff;
	send_buf[1 + MT_RPC_POS_DAT0 + 1] =
	  MT_UartCalcFCS( &send_buf[1 + MT_RPC_POS_LEN], MT_UART_HEAD_LEN + 1 );

	HalUARTWrite( 0, send_buf, MT_UART_HEAD_LEN + 1 + 2 );
}


void CustomApp_Get_Flex_Devinfo( void )
{
        CustomApp_Coordinator_Online();
}


uint8 Nvram_Write_Multiway( void )
{
	uint8 ret;
	ret = osal_nv_write( ZCD_NV_MULTIWAY, 0, sizeof( PROT_GROUP_STRUCT ), &Prot_Group_T );
	return ret;
}


void Register_Group_Multiway( void )
{
    if(Prot_Group_T.group_type == 1)//add to group
    {
        SerialApp_TxAddr_group.addrMode = (afAddrMode_t)AddrGroup;
        SerialApp_TxAddr_group.endPoint = GENERICAPP_ENDPOINT;
        SerialApp_TxAddr_group.addr.shortAddr = GENERICAPP_GROUP;

        SerialApp_TxAddr_group.addr.shortAddr = Prot_Group_T.group_id;//set group id to global variables
        GenericApp_Group.ID = Prot_Group_T.group_id;
        osal_memcpy( GenericApp_Group.name, "Group 1", 7 );
        aps_AddGroup( GENERICAPP_ENDPOINT, &GenericApp_Group );
    }
}


