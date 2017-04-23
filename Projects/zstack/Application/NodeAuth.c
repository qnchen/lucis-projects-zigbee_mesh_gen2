/**************************************************************************************************
  Filename:       NodeAuth.c
  Revised:        2017-03-19

  Description -   Serial Transfer Application node authentication related func.

**************************************************************************************************/


/*********************************************************************
 * INCLUDES
 */
#include "NodeAuth.h"
#include "OSAL_Nv.h"
#include "hal_aes.h"
#include "hal_dma.h"

#include "GenericApp.h"
#include <string.h>  

/*********************************************************************
 * MACROS
 */
#define AUTH_LIST_CACHE_SIZE           (1<<4)
#define AUTH_LIST_CACHE_ALIGN(_offset) (_offset & ~(AUTH_LIST_CACHE_SIZE -1))

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

struct
{
    uint32 version;
    uint32 auth_registered_num;
} gAuthConfig = {
    .version = 1, // increase this and handle database upgrade if new items added!
    .auth_registered_num = 0
};

static NodeRegisterEntry gListCache[AUTH_LIST_CACHE_SIZE];
static uint32 gListOffset = 0;
static uint32 gListNum = 0;

//static const uint8 DEFAULT_AES_NONCE[13] = NODE_AUTH_DEFAULT_NONCE;
//static const char  DEFAULT_AES_KEY[STATE_BLENGTH] = NODE_AUTH_DEFAULT_KEY;


/*********************************************************************
 * EXTERNAL VARIABLES
 */
 extern nodeList_t *nodeList;  

/*********************************************************************
 * LOCAL FUNCTION PROTOTYPE
 */
#define List_cache_get(_o) (List_cache_load_on_need(_o), &gListCache[(_o)-gListOffset])
#define Is_entry_deleted(_e) (0 != ((_e)->flag & NODE_REG_ENTRY_DELETED))
#define Increase_registered_num() do { Set_registered_num(gAuthConfig.auth_registered_num + 1); } while(0)
#define Decrease_registered_num() do { Set_registered_num(gAuthConfig.auth_registered_num - 1); } while(0)

static uint8 Authlist_init( uint8 load );
static void Authlist_delete_entry(uint32 index);
static bool Authlist_should_insert(uint8 const* key, uint8 len, uint32 *index);

static inline void List_cache_recreate( void );
static void List_cache_force_load(uint32 offset);
static bool List_cache_load_next( void );
static uint32 List_cache_find_by_key(uint8 const* key, uint8 len);
static uint32 List_cache_write_back( void );
static inline bool List_cache_contain(uint32 offset);
static inline void List_cache_load_on_need(uint32 offset);

static void Set_registered_num(uint32 num);

static uint16 Mail_uplink(uint8 port, uint8 cmd, uint8 len, void* buf);
static inline uint16 Mail_uplink_registered_cnt(uint8 port);
static inline uint16 Mail_uplink_key(uint8 port, uint32 index, uint8 const* key);
static inline uint16 Mail_uplink_notify(uint8 port, uint8 const* extended_addr);

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */
 
uint8 Node_auth_init( void )
{
    uint8 rc;
    if ( SUCCESS == (rc = osal_nv_item_init(APP_NV_AUTH_GLOBAL_CONFIG, sizeof(gAuthConfig), &gAuthConfig)) ) 
    {
        osal_nv_read(APP_NV_AUTH_GLOBAL_CONFIG, 0, sizeof(gAuthConfig), &gAuthConfig);
    }
    else if ( NV_ITEM_UNINIT == rc )
    {
    }
    else
    {
        SystemReset();
    }
    if ( SUCCESS == (rc = osal_nv_item_init(
            APP_NV_AUTH_KEY_LIST, 
            sizeof(NodeRegisterEntry) * AUTH_LIST_MAX_LEN, 
            NULL))) 
    {
        Authlist_init(TRUE);
    }
    else if ( NV_ITEM_UNINIT == rc )
    {
        Authlist_init(FALSE);
    }
    else
    {
        SystemReset();
    }
    return SUCCESS;
}



void Node_auth_wipe( void )
{
    osal_nv_delete(APP_NV_AUTH_GLOBAL_CONFIG, sizeof(gAuthConfig));
    osal_nv_delete(APP_NV_AUTH_KEY_LIST, sizeof(NodeRegisterEntry) * AUTH_LIST_MAX_LEN);
    Node_auth_init();
}



static bool Authlist_should_insert(uint8 const* key, uint8 keylen, uint32 *index)
{
    if (keylen != AUTH_LIST_KEY_LEN) {
        *index = AUTH_LIST_INVALID_INDEX;
        return 0;
    }
    uint32 avail = AUTH_LIST_INVALID_INDEX;
    uint32 idx;
    List_cache_force_load(0);
    do {
        for (idx = 0; 
             idx + gListOffset < AUTH_LIST_MAX_LEN && idx < gListNum ;
             ++idx)
        {
            if (avail == AUTH_LIST_INVALID_INDEX && 
                Is_entry_deleted(&gListCache[idx]))
            {
                avail = idx + gListOffset;
            }
            else if (!Is_entry_deleted(&gListCache[idx]) &&
                        osal_memcmp(key, gListCache[idx].key, keylen) )
            {
                *index = idx + gListOffset;;
                return FALSE;
            }
        }
    } while (List_cache_load_next());

    *index = avail;
    return avail != AUTH_LIST_INVALID_INDEX;
}


uint8 Authlist_add(uint8 const* buf, uint8 len, uint32 *index)
{
    uint32 avail;
    NodeRegisterEntry* e;

    if (len != AUTH_LIST_KEY_LEN) return 0;

    List_cache_force_load(0);

    if (Authlist_should_insert(buf, len, &avail))
    {
        e = List_cache_get(avail);
        e->flag &= ~NODE_REG_ENTRY_DELETED;
        osal_memcpy(e->key, buf, len);
        
        Increase_registered_num();
        List_cache_write_back();
        if(index) *index = avail;
        return Authlist_item_cnt();
    }
    else if (avail == AUTH_LIST_INVALID_INDEX)
    {    
        return 0;
    }
    else
    {
        return Authlist_item_cnt(); 
    }
}


uint8 Authlist_get(uint32 index, uint8* buf, uint8* len)
{
    NodeRegisterEntry const* e;

    if (*len < AUTH_LIST_KEY_LEN) return 0;
    else *len = AUTH_LIST_KEY_LEN;

    List_cache_load_on_need(index);

    e = List_cache_get(index);
    if (NULL == e || Is_entry_deleted(e))
    {
        return 0;
    }
    
    osal_memcpy(buf, e->key, AUTH_LIST_KEY_LEN);
    return Authlist_item_cnt();
}

static void Authlist_delete_entry(uint32 index)
{
    NodeRegisterEntry *last_entry_p, *to_del;
    NodeRegisterEntry last_entry_copy;
    uint32 last_index = Authlist_item_cnt() - 1;

    if (index >= Authlist_item_cnt() || 
        index >= AUTH_LIST_MAX_LEN)
    {
        return;
    }
    if (index != last_index) {
        // not last entry, need to fill the blank
        last_entry_p = List_cache_get(last_index);
        osal_memcpy(&last_entry_copy, last_entry_p, sizeof(NodeRegisterEntry));
        last_entry_p->flag |= NODE_REG_ENTRY_DELETED;
        osal_memset(last_entry_p->key, 0xff, AUTH_LIST_KEY_LEN);
        List_cache_write_back();
    } else {
        last_entry_p = NULL;
    }

    to_del = List_cache_get(index);
    if (last_entry_p != NULL) {
        osal_memcpy(to_del, &last_entry_copy, sizeof(NodeRegisterEntry));
    } else {
        to_del->flag |= NODE_REG_ENTRY_DELETED;
        osal_memset(to_del->key, 0xff, AUTH_LIST_KEY_LEN);
    }
    Decrease_registered_num();
    List_cache_write_back();
}


void Authlist_del(uint8 const* key, uint8 keylen, uint32* index)
{
    uint32 to_del;
    if(AUTH_LIST_INVALID_INDEX != (to_del = Authlist_find_by_key(key, keylen)))
    {
        Authlist_delete_entry(to_del);
    }
}

uint8 Authlist_item_cnt( void )
{
    return gAuthConfig.auth_registered_num;
}


uint32 Authlist_find_by_key(uint8 const * key, uint32 keylen)
{
    uint32 rc = AUTH_LIST_INVALID_INDEX;
    if (keylen != AUTH_LIST_KEY_LEN) return rc;
    
    List_cache_force_load(0);
    
    do {
        if(AUTH_LIST_INVALID_INDEX != (rc = List_cache_find_by_key(key, keylen)))
        {
            break;
        }
    } while (List_cache_load_next());
    return rc;
}


void Node_auth_uart_msg_process(uint8 port, uint8 cmd0, uint8 cmd1, uint8 const* buf, uint8 len)  //chaokw
{
    if (cmd0 != NODE_AUTH_MSG_DIR_DOWNLINK) return;

    uint32 idx;
    uint8 i;
    uint8 key[AUTH_LIST_KEY_LEN+2] = {0};
    uint8 keylen = AUTH_LIST_KEY_LEN;
    nodeList_t *nodeSearch;
    uint8 deltype;


    switch (cmd1) 
    {
        case NODE_AUTH_MSG_CMD_COUNT: 
            Mail_uplink_registered_cnt(port);
            break;
        case NODE_AUTH_MSG_CMD_GET:
            if (len != 4) break;
            for (idx = i = 0; i < sizeof(idx); ++i)
            {
                idx <<= 8;
                idx |= buf[i];
            }
            if (Authlist_get(idx, key, &keylen) > 0)
            {
                for (nodeSearch = nodeList; (nodeSearch != NULL);nodeSearch = nodeSearch->nextDesc)
                {
                    if (memcmp(key, &nodeSearch->devAddr64, 8) == 0)
                    {
				key[AUTH_LIST_KEY_LEN] = BREAK_UINT16(nodeSearch->devAddr16.addr.shortAddr, 1); 
				key[AUTH_LIST_KEY_LEN+1] = BREAK_UINT16(nodeSearch->devAddr16.addr.shortAddr, 0);
                    }	
                }		
                Mail_uplink_key(port, idx, key);
            }				
            break;
        case NODE_AUTH_MSG_CMD_WIPE:
            Set_registered_num(0);
            Node_auth_wipe();
            Mail_uplink_registered_cnt(port);
            break;
        case NODE_AUTH_MSG_CMD_ADD:
            if (Authlist_add(buf, len, &idx) > 0) {
                Mail_uplink_registered_cnt(port);
            }
            break;
        case NODE_AUTH_MSG_CMD_DEL:
            deltype = *buf++;
	     if (0x01 == deltype)
	     {
	     	Authlist_del(buf, len-1, &idx);
              Mail_uplink_registered_cnt(port);
	     }
	     else if (0xff == deltype)
	     {
	       Set_registered_num(0);
              Node_auth_wipe();
              Mail_uplink_registered_cnt(port);
	     }
            break;
        case NODE_AUTH_MSG_CMD_RESET_NET:
            SystemResetSoft();
            break;
        default:
            break;
    }
    return;
}

/*********************************************************************
 * LOCAL VARIABLES
 */

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void Set_registered_num(uint32 num)
{
    gAuthConfig.auth_registered_num = num;
    osal_nv_write(APP_NV_AUTH_GLOBAL_CONFIG, 0, sizeof(gAuthConfig), (void*)&gAuthConfig);
}


static uint16 Mail_uplink(uint8 port, uint8 cmd, uint8 len, void* buf)
{
    uint8 uartpbuf[20];
    uint16 rc;
    
    if (uartpbuf == NULL || buf == NULL || len == 0)
        return 0;

    uartpbuf[0] = 0xFE;
    uartpbuf[1] = len;
    uartpbuf[2] = NODE_AUTH_MSG_DIR_UPLINK;
    uartpbuf[3] = cmd;
    osal_memcpy(&uartpbuf[4], buf, len);

#ifdef OPEN_FCS
    uartpbuf[len+4] = MT_UartCalcFCS(&uartpbuf[1], len+3);
#else
    uartpbuf[len+4] = DEFAULTFCS;  
#endif
    rc = HalUARTWrite(port, uartpbuf, len+5);    
    return rc;

}


static inline uint16 Mail_uplink_registered_cnt(uint8 port)
{
    uint32 n = Authlist_item_cnt();
    uint32 n_be =
        ((n >> 24) & 0xff) |
        ((n >>  8) & 0xff00) |
        ((n <<  8) & 0xff0000) |
        ((n << 24) & 0xff000000);
    return Mail_uplink(port, NODE_AUTH_MSG_CMD_COUNT, 4, &n_be);
}


static inline uint16 Mail_uplink_key(uint8 port, uint32 index, uint8 const* key)  //chaokw
{
    uint8 buf[sizeof(index) + AUTH_LIST_KEY_LEN+2];
    uint8 i;

    for (i = 0; i < sizeof(index); i++) {
        buf[i] = (uint8)((index >> (8 * (sizeof(index) - i - 1))) & 0xff);
    }

    osal_memcpy(&buf[i], key, AUTH_LIST_KEY_LEN+2);
    return Mail_uplink(port, NODE_AUTH_MSG_CMD_GET, sizeof(buf), buf);
}


static inline uint16 Mail_uplink_notify(uint8 port, uint8 const* extended_addr)
{
    uint8 buf[8];
    osal_memcpy(buf, extended_addr, 8);
    return Mail_uplink(port, NODE_AUTH_MSG_CMD_REFUSE, sizeof(buf), buf);
}



static inline void List_cache_recreate( void ) {
    osal_memset(gListCache, 0xff, AUTH_LIST_CACHE_SIZE*sizeof(NodeRegisterEntry));
}


static inline void List_cache_load_on_need(uint32 offset) 
{
    if (!List_cache_contain(offset))
        List_cache_force_load(AUTH_LIST_CACHE_ALIGN(offset));
}


static uint32 List_cache_write_back()
{
    return osal_nv_write(
        APP_NV_AUTH_KEY_LIST, 
        gListOffset * sizeof(NodeRegisterEntry), 
        gListNum * sizeof(NodeRegisterEntry), 
        gListCache);
}

static inline bool List_cache_contain(uint32 index)
{
    return (gListCache != NULL) && 
            (index >= gListOffset) && 
            (index < gListOffset + gListNum);
}

static uint32 List_cache_find_by_key(uint8 const* key, uint8 keylen)
{
    uint32 idx;
    uint32 rc = AUTH_LIST_INVALID_INDEX;
    if (gListCache == NULL) return rc;
    for (idx = 0; 
         idx + gListOffset < AUTH_LIST_MAX_LEN && idx < gListNum ;
         ++idx)
    {
        if ( !Is_entry_deleted(&gListCache[idx]) &&
             osal_memcmp(key, gListCache[idx].key, keylen) 
           )
        {
            rc = idx + gListOffset;
            break;
        }
    }
    return rc;
}

static bool List_cache_load_next( void )
{
    if (gListOffset + gListNum >= AUTH_LIST_MAX_LEN)
        return FALSE;
    List_cache_force_load(gListOffset + gListNum);
    return TRUE;
}


static void List_cache_force_load(uint32 index) {
    gListNum = (
        index + AUTH_LIST_CACHE_SIZE > AUTH_LIST_MAX_LEN ?
        AUTH_LIST_MAX_LEN - index :
        AUTH_LIST_CACHE_SIZE
    );
    gListOffset = index;
    if ( SUCCESS != osal_nv_read(
            APP_NV_AUTH_KEY_LIST, 
            index * sizeof(NodeRegisterEntry), 
            gListNum * sizeof(NodeRegisterEntry),
            gListCache))
    {
        List_cache_recreate();
        gListNum = 0;
        gListOffset = AUTH_LIST_INVALID_INDEX;
    }
    else if ( gListNum != AUTH_LIST_CACHE_SIZE ) {
        osal_memset(&gListCache[gListNum], 0xff, (AUTH_LIST_CACHE_SIZE - gListNum) * sizeof(gListCache[0]));
    }
}


static uint8 Authlist_init( uint8 load )
{
    List_cache_recreate();
    if (load) List_cache_force_load(0);
    return SUCCESS;
}

bool Authlist_verify_device(uint16 ShortAddress, uint8 *ExtendedAddress,
                                uint8 CapabilityFlags, uint8 type)
{
    uint8 sig[AUTH_LIST_KEY_LEN];
	
    osal_memcpy(sig, ExtendedAddress, AUTH_LIST_KEY_LEN);
    if (AUTH_LIST_INVALID_INDEX != 
            Authlist_find_by_key(sig, AUTH_LIST_KEY_LEN))
        return TRUE;

    Mail_uplink_notify(0, ExtendedAddress);
    return FALSE;
}
