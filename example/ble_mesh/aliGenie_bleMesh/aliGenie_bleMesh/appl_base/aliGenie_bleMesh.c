#include "rf_phy_driver.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "pwrmgr.h"

#include "gatt.h"
#include "gatt_uuid.h"
#include "hci.h"
#include "hci_tl.h"
#include "ll.h"

#include "gapgattserver.h"
#include "gattservapp.h"
#include "devinfoservice.h"
#include "simpleGATTprofile_ota.h"
#include "model_state_handler_pl.h"
#include "peripheral.h"
#include "gapbondmgr.h"

#include "aliGenie_bleMesh.h"
#include "uart.h"
#include "sha256.h"
#include "ali_genie_profile.h"
#include "aliGenie_led_light.h"


#include "EM_os.h"
#include "EM_debug.h"
#include "EM_timer.h"

#include "MS_common.h"
#include "MS_prov_api.h"

#include "nvs.h"

#include "blebrr.h"

#include "cliface.h"

#include "mesh_clients.h"
#include "access_extern.h"
#include "flash.h"
extern void appl_mesh(void);
extern void appl_dump_bytes(UCHAR* buffer, UINT16 length);

#define BLEMESH_START_DEVICE_EVT            0x0001
#define BLEMESH_UART_RX_EVT                 0x0002
#define BLEMESH_GAP_SCANENABLED             0x0004
extern PROV_DEVICE_S UI_lprov_device;
#define BLEMESH_ECDH_PROCESS                0x0008

#define BLEMESH_LIGHT_PRCESS_EVT            0x0010
#define BLEMESH_GAP_TERMINATE               0x0010
#define BLEMESH_GAP_MSG_EVT                 0x0400
#define ON_OFF_RESET_FLASH_ADDR             0x11003000

/*********************************************************************
    MACROS
*/

/*********************************************************************
    CONSTANTS
*/

/*********************************************************************
    TYPEDEFS
*/

/*********************************************************************
    GLOBAL VARIABLES
*/

API_RESULT cli_demo_help(UINT32 argc, UCHAR* argv[]);
API_RESULT cli_internal_status(UINT32 argc, UCHAR* argv[]);
API_RESULT cli_demo_reset(UINT32 argc, UCHAR* argv[]);
API_RESULT cli_disp_key(UINT32 argc, UCHAR* argv[]);


UCHAR cmdstr[64];
UCHAR cmdlen;
DECL_CONST CLI_COMMAND cli_cmd_list[] =
{
    /* Help */
    { "help", "Help on this CLI Demo Menu", cli_demo_help },

    /* Reset */
    { "reset", "Reset the device", cli_demo_reset },

    /* internal status */
    { "status", "internal status", cli_internal_status },

    /*display key */
    { "key", "display key", cli_disp_key }
};

/*********************************************************************
    EXTERNAL VARIABLES
*/
extern PROV_DEVICE_S UI_lprov_device;


/*********************************************************************
    EXTERNAL FUNCTIONS
*/

extern API_RESULT blebrr_handle_le_connection_pl
(
    uint16_t  conn_idx,
    uint16_t  conn_hndl,
    uint8_t   peer_addr_type,
    uint8_t*    peer_addr
);

extern API_RESULT blebrr_handle_le_disconnection_pl
(
    uint16_t  conn_idx,
    uint16_t  conn_hndl,
    uint8_t   reason
);

void blebrr_handle_evt_adv_complete (UINT8 enable);
void blebrr_handle_evt_adv_report (gapDeviceInfoEvent_t* adv);
void blebrr_handle_evt_scan_complete (UINT8 enable);

API_RESULT blebrr_handle_le_connection_pl(uint16_t  conn_idx, uint16_t  conn_hndl, uint8_t   peer_addr_type, uint8_t*    peer_addr);
API_RESULT blebrr_handle_le_disconnection_pl(uint16_t  conn_idx, uint16_t  conn_hndl, uint8_t   reason);

extern void appl_sample (void);
extern void appl_dump_bytes(UCHAR* buffer, UINT16 length);


static void bleMesh_ProcessOSALMsg( osal_event_hdr_t* pMsg );
static void bleMesh_ProcessGAPMsg( gapEventHdr_t* pMsg );
static void bleMesh_ProcessL2CAPMsg( gapEventHdr_t* pMsg );
static void bleMesh_ProcessGATTMsg( gattMsgEvent_t* pMsg );
void bleMesh_uart_init(void);

/*********************************************************************
    LOCAL VARIABLES
*/
uint8 bleMesh_TaskID;   // Task ID for internal task/event processing
uint8       g_dev_reset_flg;


// GAP - SCAN RSP data (max size = 31 bytes)
//static uint8 scanRspData[B_MAX_ADV_LEN];

// GAP GATT Attributes
static uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "PHYPLUS ALI MSH";

static gapDevDiscReq_t bleMesh_scanparam;
static gapAdvertisingParams_t bleMesh_advparam;

static UCHAR bleMesh_DiscCancel = FALSE;             // HZF�� not use???

/*********************************************************************
    LOCAL FUNCTIONS
*/
static void bleMesh_ProcessOSALMsg( osal_event_hdr_t* pMsg );
static void bleMesh_ProcessGAPMsg( gapEventHdr_t* pMsg );
static void bleMesh_ProcessL2CAPMsg( gapEventHdr_t* pMsg );
static void bleMesh_ProcessGATTMsg( gattMsgEvent_t* pMsg );
static void bleMesh_uart_init(void);

void UI_set_uuid_octet (UCHAR uuid_0);
void on_off_reset_process(void);
uint8 bleMesh_TaskID;
/*********************************************************************
    PROFILE CALLBACKS
*/

/*********************************************************************
    PUBLIC FUNCTIONS
*/

/*********************************************************************
    @fn      bleMesh_Init

    @brief   Initialization function for the Simple BLE Peripheral App Task.
            This is called during initialization and should contain
            any application specific initialization (ie. hardware
            initialization/setup, table initialization, power up
            notificaiton ... ).

    @param   task_id - the ID assigned by OSAL.  This ID should be
                      used to send messages and set timers.

    @return  none
*/
void bleMesh_Init( uint8 task_id )
{
//    bStatus_t ret;
    bleMesh_TaskID = task_id;
    //ppsp_impl_ini();//mesh ota
    // Register for direct HCI messages
    //HCI_GAPTaskRegister(bleMesh_TaskID);
    GAP_ParamsInit (bleMesh_TaskID, (GAP_PROFILE_PERIPHERAL | GAP_PROFILE_CENTRAL));
    /* printf ("Ret - %02x\r\n", ret); */
    GAP_CentDevMgrInit(0x80);
    /* printf ("Ret - %02x\r\n", ret); */
    GAP_PeriDevMgrInit();
    /* printf ("Ret - %02x\n", ret); */
    GAP_CentConnRegister();
    // Set the GAP Characteristics
    GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName );
    // Initialize GATT attributes
    GGS_AddService( GATT_ALL_SERVICES );            // GAP
    GATTServApp_AddService( GATT_ALL_SERVICES );    // GATT attributes
    DevInfo_AddService();                           // Device Information Service
    GATTServApp_RegisterForMsg(task_id);
    bleMesh_uart_init();
    osal_set_event( bleMesh_TaskID, BLEMESH_START_DEVICE_EVT );
    TRNG_INIT();
    // For DLE
    llInitFeatureSet2MPHY(TRUE);
    llInitFeatureSetDLE(TRUE);
    // HCI_LE_SetDefaultPhyMode(0,0,0x01,0x01);
    on_off_reset_process();
}


/*********************************************************************
    @fn      bleMesh_ProcessEvent

    @brief   Simple BLE Peripheral Application Task event processor.  This function
            is called to process all events for the task.  Events
            include timers, messages and any other user defined events.

    @param   task_id  - The OSAL assigned task ID.
    @param   events - events to process.  This is a bit map and can
                     contain more than one event.

    @return  events not processed
*/
uint16 bleMesh_ProcessEvent( uint8 task_id, uint16 events )
{
    VOID task_id; // OSAL required parameter that isn't used in this function
//    hciStatus_t ret;

    /* printf("\n >> bleMesh_ProcessEvent- TaskID %d : events 0x%04X\n", task_id, events); */

    if ( events & SYS_EVENT_MSG )
    {
        uint8* pMsg;

        if ( (pMsg = osal_msg_receive( bleMesh_TaskID )) != NULL )
        {
            bleMesh_ProcessOSALMsg( (osal_event_hdr_t*)pMsg );
            // Release the OSAL message
            VOID osal_msg_deallocate( pMsg );
        }

        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & BLEMESH_LIGHT_PRCESS_EVT)
    {
        light_blink_porcess_evt();
        return (events ^ BLEMESH_LIGHT_PRCESS_EVT);
    }

    if ( events & BLEMESH_START_DEVICE_EVT )
    {
        /* Register for GATT Client */
        GATT_InitClient();
        GATT_RegisterForInd(bleMesh_TaskID);
        light_init();
        light_blink_evt_cfg(bleMesh_TaskID,BLEMESH_LIGHT_PRCESS_EVT);
        //LIGHT_ONLY_RED_ON;//change by johhn
        CLI_init((CLI_COMMAND*)cli_cmd_list,(sizeof (cli_cmd_list)/sizeof(CLI_COMMAND)));
        gen_aligenie_auth_val();
        appl_mesh();
        return ( events ^ BLEMESH_START_DEVICE_EVT );
    }

    if (events & BLEMESH_UART_RX_EVT)
    {
        if ('\r' == cmdstr[cmdlen - 1] || '\n' == cmdstr[cmdlen - 1])
        {
            cmdstr[cmdlen - 1] = '\0';
            printf("%s", cmdstr);
            CLI_process_line_manual
            (
                cmdstr,
                cmdlen
            );
            cmdlen = 0;
        }

        return ( events ^ BLEMESH_UART_RX_EVT );
    }

    if (events & BLEMESH_GAP_SCANENABLED)
    {
        if(BLEBRR_STATE_ADV_ENABLED != BLEBRR_GET_STATE())
            blebrr_handle_evt_scan_complete(0x01);

        return (events ^ BLEMESH_GAP_SCANENABLED);
    }

    if (events & BLEMESH_GAP_TERMINATE)
    {
        blebrr_disconnect_pl();
        return (events ^ BLEMESH_GAP_TERMINATE);
    }

    if (events & BLEMESH_ECDH_PROCESS)
    {
//      cry_ecdh_process_secret();
        return (events ^ BLEMESH_ECDH_PROCESS);
    }

    // Discard unknown events
    return 0;
}

/*********************************************************************
    @fn      bleMesh_ProcessOSALMsg

    @brief   Process an incoming task message.

    @param   pMsg - message to process

    @return  none
*/
static void bleMesh_ProcessOSALMsg( osal_event_hdr_t* pMsg )
{
    switch ( pMsg->event )
    {
    case GAP_MSG_EVENT:
        bleMesh_ProcessGAPMsg( (gapEventHdr_t*)pMsg );
        break;

    case L2CAP_SIGNAL_EVENT:
        bleMesh_ProcessL2CAPMsg( (gapEventHdr_t*)pMsg );
        break;

    case GATT_MSG_EVENT:
        bleMesh_ProcessGATTMsg( (gattMsgEvent_t*)pMsg );
        /* Invoke the Mesh Client GATT Msg Handler */
        #ifdef BLE_CLIENT_ROLE
        mesh_client_process_gattMsg((gattMsgEvent_t*)pMsg, bleMesh_TaskID);
        #endif
        break;

    default:
        break;
    }
}

static void bleMesh_ProcessGATTMsg( gattMsgEvent_t* pMsg )
{
//    bStatus_t ret;

    // Process the GATT server message
    switch ( pMsg->method )
    {
    case ATT_EXCHANGE_MTU_RSP:
        break;

    default:
        break;
    }
}

static void bleMesh_ProcessL2CAPMsg( gapEventHdr_t* pMsg )
{
    l2capSignalEvent_t* pPkt = (l2capSignalEvent_t*)pMsg;

    // Process the Parameter Update Response
    if ( pPkt->opcode == L2CAP_PARAM_UPDATE_RSP )
    {
        l2capParamUpdateRsp_t* pRsp = (l2capParamUpdateRsp_t*)&(pPkt->cmd.updateRsp);

        if (pRsp->result == L2CAP_CONN_PARAMS_ACCEPTED )
        {
            printf ("L2CAP Connection Parameter Updated!\n");
        }
    }
}

static void bleMesh_ProcessGAPMsg( gapEventHdr_t* pMsg )
{
    hciStatus_t ret;
    gapDeviceInfoEvent_t* dev;

    if ((pMsg->hdr.status != SUCCESS)
            && (pMsg->hdr.status != bleGAPUserCanceled || pMsg->opcode != GAP_DEVICE_DISCOVERY_EVENT))
    {
//        printf ("GAP Event - %02X, status = %X\r\n", pMsg->opcode, pMsg->hdr.status);
        //cli_internal_status(0,0);
    }

    switch (pMsg->opcode)
    {
    case GAP_DEVICE_INIT_DONE_EVENT:
    {
        gapDeviceInitDoneEvent_t* pPkt = (gapDeviceInitDoneEvent_t*) pMsg;

        if ( pPkt->hdr.status == SUCCESS )
        {
            // Save BD Address information in pPkt->devAddr for B_ADDR_LEN
        }
    }
    break;

    case GAP_MAKE_DISCOVERABLE_DONE_EVENT:  //adv start data ready
        if (SUCCESS != pMsg->hdr.status)
        {
            //printf ("MDReq\n");
            ret = GAP_MakeDiscoverable(bleMesh_TaskID, &bleMesh_advparam);

            if (SUCCESS != ret)
            {
                printf ("GAP_MakeDiscoverable Failed - %d\n", ret);
            }
        }
        else
        {
            blebrr_handle_evt_adv_complete(1);
//                blebrr_pl_advertise_setup(1);
        }

        break;

    case GAP_END_DISCOVERABLE_DONE_EVENT:

        //printf ("->%d\r\n", pMsg->opcode);
        //printf ("ED Status - %d", pMsg->hdr.status);
        if (SUCCESS != pMsg->hdr.status)
        {
            //printf ("EDReq\n");
            ret = GAP_EndDiscoverable(bleMesh_TaskID);

            if (SUCCESS != ret)
            {
                printf ("GAP_EndDiscoverable Failed - %d\n", ret);
            }
        }
        else
        {
            blebrr_handle_evt_adv_complete(0);
        }

        break;

    case GAP_DEVICE_DISCOVERY_EVENT:

//            printf("\r\nIn GAP_DEVICE_DISCOVERY_EVENT...\r\n");
        if (TRUE == bleMesh_DiscCancel)
        {
            if ((bleGAPUserCanceled != pMsg->hdr.status) && (SUCCESS != pMsg->hdr.status))
            {
                GAP_DeviceDiscoveryCancel(bleMesh_TaskID);
            }
            else
            {
                bleMesh_DiscCancel = FALSE;
                blebrr_handle_evt_scan_complete(0);
            }
        }
        else
        {
//                printf (">>> %d\r\n", pMsg->hdr.status);
            if (SUCCESS == pMsg->hdr.status)
            {
                GAP_DeviceDiscoveryRequest(&bleMesh_scanparam);
            }
            else if((LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE == pMsg->hdr.status) || (bleMemAllocError == pMsg->hdr.status) )
            {
                printf ("GAP_DeviceDiscoveryRequest Retry - %02X\r\n", pMsg->hdr.status);
                GAP_DeviceDiscoveryRequest(&bleMesh_scanparam);
            }
        }

        break;

    case GAP_DEVICE_INFO_EVENT:
        dev = (gapDeviceInfoEvent_t*)pMsg;
        blebrr_handle_evt_adv_report(dev);
        break;

    case GAP_LINK_ESTABLISHED_EVENT:
    {
        blebrr_timer_stop();
        gapEstLinkReqEvent_t* pPkt = (gapEstLinkReqEvent_t*)pMsg;
        printf("\n GAP_LINK_ESTABLISHED_EVENT received! \n");

        if ( pPkt->hdr.status == SUCCESS )
        {
            /* Send Connection Complete to  Ble Bearer PL layer */
            blebrr_handle_le_connection_pl
            (
                0x00, /* Dummy Static Connection Index */
                pPkt->connectionHandle,
                pPkt->devAddrType,
                pPkt->devAddr
            );
        }
    }
    break;

    case GAP_LINK_TERMINATED_EVENT:
    {
        gapTerminateLinkEvent_t* pPkt = (gapTerminateLinkEvent_t*)pMsg;
        /* printf("\n GAP_LINK_TERMINATED_EVENT received! \n"); */
        blebrr_handle_le_disconnection_pl
        (
            0x00, /* Dummy Static Connection Index */
            pPkt->connectionHandle,
            pPkt->reason
        );
    }
    break;
    #if 0

    case GAP_LINK_PARAM_UPDATE_EVENT:
    {
        gapLinkUpdateEvent_t* pPkt = (gapLinkUpdateEvent_t*)pMsg;
        l2capParamUpdateReq_t updateReq;
        uint16 timeout = GAP_GetParamValue( TGAP_CONN_PARAM_TIMEOUT );
        printf("\n GAP_LINK_PARAM_UPDATE_EVENT received! \n");
        updateReq.intervalMin = 0x28;
        updateReq.intervalMax = 0x38;
        updateReq.slaveLatency = 0;
        updateReq.timeoutMultiplier = 0x0c80;
        L2CAP_ConnParamUpdateReq( pPkt->connectionHandle, &updateReq, bleMesh_TaskID );
    }
    break;
        #endif /* 0 */

    default:
        break;
    }
}

bStatus_t BLE_gap_set_scan_params
(
    uint8_t scan_type,
    uint16_t scan_interval,
    uint16_t scan_window,
    uint8_t scan_filterpolicy
)
{
    GAP_SetParamValue( TGAP_GEN_DISC_SCAN_WIND, scan_window );
    GAP_SetParamValue( TGAP_GEN_DISC_SCAN_INT, scan_interval );
    GAP_SetParamValue( TGAP_FILTER_ADV_REPORTS, FALSE);
    GAP_SetParamValue( TGAP_GEN_DISC_SCAN, 1000 );
    GAP_SetParamValue( TGAP_CONN_SCAN_INT,scan_interval );
    GAP_SetParamValue( TGAP_CONN_SCAN_WIND,scan_window);
    //GAP_SetParamValue( TGAP_LIM_DISC_SCAN, 0xFFFF );
    bleMesh_scanparam.activeScan = scan_type;
    bleMesh_scanparam.mode = DEVDISC_MODE_ALL; //DEVDISC_MODE_GENERAL;
    bleMesh_scanparam.whiteList = scan_filterpolicy;
    bleMesh_scanparam.taskID = bleMesh_TaskID;
    return 0x00;
}


bStatus_t BLE_gap_set_scan_enable
(
    uint8_t scan_enable
)
{
    bStatus_t ret;

    if (0x00 != scan_enable)
    {
        ret = GAP_DeviceDiscoveryRequest(&bleMesh_scanparam);

        if (0 == ret)
        {
            bleMesh_DiscCancel = FALSE;
            osal_set_event (bleMesh_TaskID, BLEMESH_GAP_SCANENABLED);
        }
    }
    else
    {
        ret = GAP_DeviceDiscoveryCancel(bleMesh_TaskID);

        if ((0 == ret) ||(bleIncorrectMode == ret))
        {
            bleMesh_DiscCancel = TRUE;
        }
    }

    return ret;
}

bStatus_t BLE_gap_set_adv_params
(
    uint8_t adv_type,
    uint16_t adv_intervalmin,
    uint16_t adv_intervalmax,
    uint8_t adv_filterpolicy
)
{
    GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MIN, adv_intervalmin );
    GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MAX, adv_intervalmax );
    GAP_SetParamValue( TGAP_GEN_DISC_ADV_MIN, 0);
    bleMesh_advparam.channelMap = GAP_ADVCHAN_ALL;
    bleMesh_advparam.eventType = adv_type;
    bleMesh_advparam.filterPolicy = adv_filterpolicy;
    bleMesh_advparam.initiatorAddrType = 0x00;
    osal_memset(bleMesh_advparam.initiatorAddr, 0x00, B_ADDR_LEN);
    return SUCCESS;
}

bStatus_t BLE_gap_set_advscanrsp_data
(
    uint8_t   type,
    uint8_t* adv_data,
    uint16_t  adv_datalen
)
{
    return GAP_UpdateAdvertisingData(bleMesh_TaskID, type, adv_datalen, adv_data);
}

bStatus_t BLE_gap_connect
(
    uint8_t   whitelist,
    uint8_t* addr,
    uint8_t   addr_type
)
{
    gapEstLinkReq_t params;
    params.taskID = bleMesh_TaskID;
    params.highDutyCycle = TRUE;
    params.whiteList = whitelist;
    params.addrTypePeer = addr_type;
    VOID osal_memcpy( params.peerAddr, addr, B_ADDR_LEN );
    return GAP_EstablishLinkReq( &params );
}

bStatus_t BLE_gap_disconnect(uint16_t   conn_handle)
{
    return GAP_TerminateLinkReq( bleMesh_TaskID, conn_handle, HCI_DISCONNECT_REMOTE_USER_TERM ) ;
}

bStatus_t BLE_gap_set_adv_enable
(
    uint8_t adv_enable
)
{
    bStatus_t ret;

    if (0x00 != adv_enable)
    {
        ret = GAP_MakeDiscoverable(bleMesh_TaskID, &bleMesh_advparam);
    }
    else
    {
        ret = GAP_EndDiscoverable(bleMesh_TaskID);
    }

    return ret;
}




static void ProcessUartData(uart_Evt_t* evt)
{
    UCHAR c_len = cmdlen + evt->len;

    if(c_len < sizeof(cmdstr))
    {
        osal_memcpy((cmdstr + cmdlen), evt->data, evt->len);
        cmdlen += evt->len;
        osal_set_event( bleMesh_TaskID, BLEMESH_UART_RX_EVT );
    }
    else
    {
        cmdlen = 0;
    }
}

void bleMesh_uart_init(void)
{
    hal_uart_deinit(UART0);
    uart_Cfg_t cfg =
    {
        .tx_pin = P9,
        .rx_pin = P10,
        .rts_pin = GPIO_DUMMY,
        .cts_pin = GPIO_DUMMY,
        .baudrate = 115200,
        .use_fifo = TRUE,
        .hw_fwctrl = FALSE,
        .use_tx_buf = FALSE,
        .parity     = FALSE,
        .evt_handler = ProcessUartData,
    };
    hal_uart_init(cfg,UART0);//uart init
}

void UI_set_uuid_octet (UCHAR uuid_0)
{
    UI_lprov_device.uuid[0] = uuid_0;
}

extern uint8 llState;
extern uint8 llSecondaryState;
extern llGlobalStatistics_t g_pmCounters;
extern UCHAR blebrr_state;
extern uint32 blebrr_advscan_timeout_count;
//extern uint32  osal_memory_statics(void);
API_RESULT cli_internal_status(UINT32 argc, UCHAR* argv[])
{
//    UINT32 index;
    MS_IGNORE_UNUSED_PARAM(argc);
    MS_IGNORE_UNUSED_PARAM(argv);
//    printf("\n===== internal status ============\n");
//    printf("llState = %d, llSecondaryState = %d\n", llState, llSecondaryState);
//
//    printf("conn_event cnt = %d, crc err count = %d\n", g_pmCounters.ll_conn_event_cnt,
//             g_pmCounters.ll_recv_crcerr_event_cnt);
//  printf("blebrr_state = %d\n", blebrr_state);
//    printf("blebrr_advscan_timeout_count = %d\n", blebrr_advscan_timeout_count);
//
//    osal_memory_statics();
//    printf("\n");
    return API_SUCCESS;
}

void on_off_reset_process(void)
{
    uint32 tmp = *(volatile uint32_t*) ON_OFF_RESET_FLASH_ADDR ;

    if(tmp==0xffffffff)
        tmp=0;

    hal_flash_erase_sector(ON_OFF_RESET_FLASH_ADDR);
    flash_write_word(ON_OFF_RESET_FLASH_ADDR, tmp+1);
    g_dev_reset_flg = tmp+1;
    WaitMs(500);
    hal_flash_erase_sector(ON_OFF_RESET_FLASH_ADDR);
}

API_RESULT cli_demo_help(UINT32 argc, UCHAR* argv[])
{
    UINT32 index;
    MS_IGNORE_UNUSED_PARAM(argc);
    MS_IGNORE_UNUSED_PARAM(argv);
    printf("\r\nCLI Demo\r\n");

    /* Print all the available commands */
    for (index = 0; index < g_cli_cmd_len; index++)
    {
        printf("    %s: %s\n",
               g_cli_cmd_list[index].cmd,
               g_cli_cmd_list[index].desc);
    }

    return API_SUCCESS;
}

API_RESULT cli_demo_reset(UINT32 argc, UCHAR* argv[])
{
    blebrr_scan_pl(FALSE);
    nvs_reset(NVS_BANK_PERSISTENT);
    MS_access_cm_reset(PROV_ROLE_DEVICE);
    printf ("Done\r\n");
    return API_SUCCESS;
}
extern API_RESULT UI_get_net_key(void );
extern API_RESULT UI_get_device_key(void );
extern API_RESULT UI_check_app_key(void);
API_RESULT cli_disp_key(UINT32 argc, UCHAR* argv[])
{
    MS_IGNORE_UNUSED_PARAM(argc);
    MS_IGNORE_UNUSED_PARAM(argv);
    UI_get_net_key();
    UI_get_device_key();
    UI_check_app_key();
    return API_SUCCESS;
}




/*********************************************************************
*********************************************************************/
