
/**************************************************************************************************
  Filename:       NodeAuth.h
  Revised:        
  Revision:       

  Description:    This file contains the node authentication related definitions.
  
**************************************************************************************************/
#ifndef NODE_AUTH_H
#define NODE_AUTH_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"
#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "GenericApp.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "MT.h"
#include "MT_UART.h"
#include "MT_RPC.h"
#include "OSAL.h"

#include "hal_drivers.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
  #include "hal_lcd.h"
#endif
#include "hal_led.h"
#include "hal_uart.h"

/*********************************************************************
 * TYPEDEFS
 */

typedef struct {
    uint16 flag;
    uint8  key[8];
} NodeRegisterEntry;


/*********************************************************************
 * CONSTANTS
 */
/*********************************************************************
 * MACROS
 */

// 2k per flash page
#define AUTH_LIST_MAX_LEN (1024/sizeof(NodeRegisterEntry))
#define AUTH_LIST_INVALID_INDEX (0xffffffff)
#define AUTH_LIST_KEY_LEN sizeof(((NodeRegisterEntry*)0)->key)

// flags
#define NODE_REG_ENTRY_DELETED 0x0001

// uart msg cmd 0
#define NODE_AUTH_MSG_DIR_UPLINK    (0x80)
#define NODE_AUTH_MSG_DIR_DOWNLINK  (0x01)

// uart msg cmd 1
#define NODE_AUTH_MSG_CMD_ADD       (0x30)   
#define NODE_AUTH_MSG_CMD_COUNT     (0x31)
#define NODE_AUTH_MSG_CMD_GET       (0x32)
#define NODE_AUTH_MSG_CMD_DEL       (0x33)

#define NODE_AUTH_MSG_CMD_WIPE      (0x38)
#define NODE_AUTH_MSG_CMD_REFUSE    (0x39)
#define NODE_AUTH_MSG_CMD_RESET_NET (0x40)


#define NODE_AUTH_DEFAULT_NONCE     {85, 74, 53, 168, 112, 117, 138, 38, 52, 230, 59, 183, 244}
#define NODE_AUTH_DEFAULT_KEY       {'@','m','I','X','I','w','s','s','5','X','7','s','k','9','A','='}

#define FOR_START_TEST_MODE		(0x50)
#define FOR_TEST_RSSI_CMD 		(0x51)
#define FOR_TEST_PING_CMD 		(0x52)
#define FOR_TEST_TRANSMIT_CMD 	(0x53)
#define FOR_STOP_TEST_MODE		(0x55)

/*********************************************************************
 * FUNCTIONS
 */
uint8 Node_auth_init( void );
void Node_auth_wipe( void );
void Node_auth_uart_msg_process(uint8 port, uint8 cmd0, uint8 cmd1, uint8 const* buf, uint8 len);


// add a key passed in by buf with length len, set index to offset and
// return number of registered keys after adding, or 0 if error happens
uint8 Authlist_add(uint8 const* buf, uint8 len, uint32 *index);

// retrieve and save a key at index to buf, which can contain at most 
// *len bytes; len will be set to the real length if key retrieved and 
// copied successfully. if key is longer than buf size, buf won't be
// changed, len will be set to length of key.
// return number of registered keys in total or 0 if error happens
uint8 Authlist_get(uint32 index, uint8* buf, uint8* len);

// delete a key at specified index.
// return number of registered keys after deleting
void Authlist_del(uint8 const* key, uint8 len, uint32* index);

// return number of registered keys
uint8 Authlist_item_cnt( void );

// find valid auth registration with given key, return index of
// matched key or AUTH_LIST_INVALID_INDEX when nothing found
uint32 Authlist_find_by_key(uint8 const * key, uint32 keylen);

// verify joining device, return TRUE to permit joining, 
// or FALSE to refuse it. a "refuse joining notify" will be 
// sent to uplink automatically before return FALSE.
bool Authlist_verify_device(uint16 ShortAddress, uint8 *ExtendedAddress,
                                uint8 CapabilityFlags, uint8 type);

#ifdef __cplusplus
}
#endif
#endif

