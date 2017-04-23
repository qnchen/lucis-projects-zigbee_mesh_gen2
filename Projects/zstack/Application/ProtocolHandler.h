/**************************************************************************************************
  Filename:       ProtocolHandler.h
  Revised:        
  Revision:       

  Description:    
  
**************************************************************************************************/
#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "GenericApp.h"



/*********************************************************************
 * LOCAL VARIABLES
 */


/*flex record group info
2Bytes	1Byte		1Byte		1Byte			1Byte		
Group ID	Group Type	Mem struct	master_location	Group Master	
*/
typedef struct
{
    uint16 group_id;
    uint8 group_type;// 1 add to group;2 remove from group
    uint8 mem_struct;//0 only flex;1 Nb and flex
    uint8 master_location;//0 master in flex;1 master in NB
    uint8 group_master;//0 slave 1 master
}PROT_GROUP_STRUCT;


extern PROT_GROUP_STRUCT Prot_Group_T;


/*********************************************************************
 * FUNCTIONS
 */
uint8 CustomApp_Send_P2P_Data( afIncomingMSGPacket_t *pkt );
uint8 CustomApp_Send_BroadCast_Data( afIncomingMSGPacket_t *pkt );
uint8 CustomApp_Send_Group_Data( afIncomingMSGPacket_t *pkt );


void CustomApp_Get_ExtAddr( void );
void CustomApp_Get_NWKInfo( void );
void CustomApp_Get_Active_Cnt( void );
void CustomApp_Get_Version( void );
void CustomApp_FW_Update( void );

void CustomApp_AF_P2P_Data_Process(afIncomingMSGPacket_t *pkt);
void CustomApp_AF_Broadcast_Data_Process(afIncomingMSGPacket_t *pkt);
void CustomApp_AF_Group_Data_Process(afIncomingMSGPacket_t *pkt);


#if defined( ZDO_COORDINATOR ) && defined( WIFI_FREQUENCY_SELECT )
void CustomApp_WifiChannelReq( void );
void CustomApp_ChannelSelect( afIncomingMSGPacket_t *MSGpkt );
#endif

void CustomApp_Set_Multiway(afIncomingMSGPacket_t *pkt);
uint8 CustomApp_coordinator_online_req ( void );
uint8 CustomApp_router_online_req ( afAddrType_t addr16 );
uint8 CustomApp_router_offline_req ( afAddrType_t addr16 );
void CustomApp_Coordinator_Online( void );
void CustomApp_Get_Flex_Devinfo( void );
uint8 Nvram_Write_Multiway( void );
void Register_Group_Multiway( void );



#ifdef __cplusplus
}
#endif
#endif

