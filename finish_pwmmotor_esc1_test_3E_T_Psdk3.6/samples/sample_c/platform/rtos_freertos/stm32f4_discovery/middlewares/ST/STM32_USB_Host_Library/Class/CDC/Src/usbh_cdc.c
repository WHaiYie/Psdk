/**
  ******************************************************************************
  * @file    usbh_cdc.c
  * @author  MCD Application Team
  * @brief   This file is the CDC Layer Handlers for USB Host CDC class.
  *
  *  @verbatim
  *
  *          ===================================================================
  *                                CDC Class Driver Description
  *          ===================================================================
  *           This driver manages the "Universal Serial Bus Class Definitions for Communications Devices
  *           Revision 1.2 November 16, 2007" and the sub-protocol specification of "Universal Serial Bus
  *           Communications Class Subclass Specification for PSTN Devices Revision 1.2 February 9, 2007"
  *           This driver implements the following aspects of the specification:
  *             - Device descriptor management
  *             - Configuration descriptor management
  *             - Enumeration as CDC device with 2 data endpoints (IN and OUT) and 1 command endpoint (IN)
  *             - Requests management (as described in section 6.2 in specification)
  *             - Abstract Control Model compliant
  *             - Union Functional collection (using 1 IN endpoint for control)
  *             - Data interface class
  *
  *  @endverbatim
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                      www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
- "stm32xxxxx_{eval}{discovery}{adafruit}_sd.c"
- "stm32xxxxx_{eval}{discovery}{adafruit}_lcd.c"
- "stm32xxxxx_{eval}{discovery}_sdram.c"
EndBSPDependencies */

#define DJI_CDC_CTRL_BINTERFACE_NUMBER 4
#define DJI_CDC_DATA_BINTERFACE_NUMBER 5
#define DJI_CDC_DATA_ALT_SETTINGS      0

/* Includes ------------------------------------------------------------------*/
#include "usbh_cdc.h"

/** @addtogroup USBH_LIB
* @{
*/

/** @addtogroup USBH_CLASS
* @{
*/

/** @addtogroup USBH_CDC_CLASS
* @{
*/

/** @defgroup USBH_CDC_CORE
* @brief    This file includes CDC Layer Handlers for USB Host CDC class.
* @{
*/

/** @defgroup USBH_CDC_CORE_Private_TypesDefinitions
* @{
*/
/**
* @}
*/


/** @defgroup USBH_CDC_CORE_Private_Defines
* @{
*/
#define USBH_CDC_BUFFER_SIZE                 1024
/**
* @}
*/


/** @defgroup USBH_CDC_CORE_Private_Macros
* @{
*/
/**
* @}
*/


/** @defgroup USBH_CDC_CORE_Private_Variables
* @{
*/
/**
* @}
*/


/** @defgroup USBH_CDC_CORE_Private_FunctionPrototypes
* @{
*/

static USBH_StatusTypeDef USBH_CDC_InterfaceInit(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_CDC_InterfaceDeInit(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_CDC_Process(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_CDC_SOFProcess(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_CDC_ClassRequest(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost,
                                        CDC_LineCodingTypeDef *linecoding);

static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost,
                                        CDC_LineCodingTypeDef *linecoding);

static void CDC_ProcessTransmission(USBH_HandleTypeDef *phost);

static void CDC_ProcessReception(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef CDC_Class =
    {
        "DJI_CDC",
        DJI_CDC_CLASS,
        USBH_CDC_InterfaceInit,
        USBH_CDC_InterfaceDeInit,
        USBH_CDC_ClassRequest,
        USBH_CDC_Process,
        USBH_CDC_SOFProcess,
        NULL,
    };
/**
* @}
*/
/* USER CODE BEGIN Data queue init */
#define ACM_RECV_BUFFER_SIZE 1024
#define ACM_SEND_BUFFER_SIZE 2048

QueueHandle_t ACMDataRecvQueue;
QueueHandle_t ACMDataSendQueue;
#if USE_USB_HOST_UART
CCMRAM static uint8_t cdcBuff[1024];
#endif

void USBH_CDC_DataQueueInit(void)
{
    ACMDataRecvQueue = xQueueCreate(ACM_RECV_BUFFER_SIZE, sizeof(uint8_t));
    ACMDataSendQueue = xQueueCreate(ACM_SEND_BUFFER_SIZE, sizeof(uint8_t));
}

void USBH_CDC_WriteData(const uint8_t *buf, uint32_t len, uint32_t *realLen)
{
#ifdef USE_USB_HOST_UART
    *realLen = len;
    while (len--) {
        xQueueSend(ACMDataSendQueue, buf++, 0);
    }
#endif
}

void USBH_CDC_ReadData(uint8_t *buf, uint32_t len, uint32_t *realLen)
{
#ifdef USE_USB_HOST_UART
    uint8_t recvByte;
    *realLen = 0;
    for (int cnt = 0; cnt < len; cnt++) {
        if (xQueueReceive(ACMDataRecvQueue, &recvByte, 0)) {
            buf[*realLen] = recvByte;
            (*realLen)++;
        } else {
            break;
        }
    }
#endif
}

/* USER CODE END Data queue init */

/** @defgroup USBH_CDC_CORE_Private_Functions
* @{
*/

/**
  * @brief  USBH_CDC_InterfaceInit
  *         The function init the CDC class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_CDC_InterfaceInit(USBH_HandleTypeDef *phost)
{

    USBH_StatusTypeDef status;
    uint8_t interface;
    CDC_HandleTypeDef *CDC_Handle;

    interface = USBH_FindInterface(phost, COMMUNICATION_INTERFACE_CLASS_CODE,
                                   ABSTRACT_CONTROL_MODEL, COMMON_AT_COMMAND);
    USBH_DbgLog("DJI USB ctrl interface : %d", interface);
    if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) /* No Valid Interface */
    {
        USBH_DbgLog("Cannot Find the interface for Communication Interface Class, name:%s", phost->pActiveClass->Name);
        return USBH_FAIL;
    }

    status = USBH_SelectInterface(phost, interface);

    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    phost->pActiveClass->pData = (CDC_HandleTypeDef *) USBH_malloc(sizeof(CDC_HandleTypeDef));
    CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (CDC_Handle == NULL) {
        USBH_DbgLog("Cannot allocate memory for CDC Handle");
        return USBH_FAIL;
    }

    /* Initialize cdc handler */
    USBH_memset(CDC_Handle, 0, sizeof(CDC_HandleTypeDef));

    /*Collect the notification endpoint address and length*/
    if (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) {
        CDC_Handle->CommItf.NotifEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        CDC_Handle->CommItf.NotifEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    }

    /*Allocate the length for host channel number in*/
    CDC_Handle->CommItf.NotifPipe = USBH_AllocPipe(phost, CDC_Handle->CommItf.NotifEp);

    /* Open pipe for Notification endpoint */
    USBH_OpenPipe(phost, CDC_Handle->CommItf.NotifPipe, CDC_Handle->CommItf.NotifEp,
                  phost->device.address, phost->device.speed, USB_EP_TYPE_INTR,
                  CDC_Handle->CommItf.NotifEpSize);

    USBH_LL_SetToggle(phost, CDC_Handle->CommItf.NotifPipe, 0U);

    /*
    interface = USBH_FindInterface(phost, DATA_INTERFACE_CLASS_CODE,
                                   RESERVED, NO_CLASS_SPECIFIC_PROTOCOL_CODE);
    */
    interface = USBH_FindInterfaceIndex(phost, DJI_CDC_DATA_BINTERFACE_NUMBER, DJI_CDC_DATA_ALT_SETTINGS);
    if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) /* No Valid Interface */
    {
        USBH_DbgLog("Cannot Find the interface for Data Interface Class, name:%s", phost->pActiveClass->Name);
        return USBH_FAIL;
    }

    /*Collect the class specific endpoint address and length*/
    if (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) {
        CDC_Handle->DataItf.InEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        CDC_Handle->DataItf.InEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    } else {
        CDC_Handle->DataItf.OutEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
        CDC_Handle->DataItf.OutEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    }

    if (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress & 0x80U) {
        CDC_Handle->DataItf.InEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress;
        CDC_Handle->DataItf.InEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
    } else {
        CDC_Handle->DataItf.OutEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].bEndpointAddress;
        CDC_Handle->DataItf.OutEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[1].wMaxPacketSize;
    }

    /*Allocate the length for host channel number out*/
    CDC_Handle->DataItf.OutPipe = USBH_AllocPipe(phost, CDC_Handle->DataItf.OutEp);

    /*Allocate the length for host channel number in*/
    CDC_Handle->DataItf.InPipe = USBH_AllocPipe(phost, CDC_Handle->DataItf.InEp);

    /* Open channel for OUT endpoint */
    USBH_OpenPipe(phost, CDC_Handle->DataItf.OutPipe, CDC_Handle->DataItf.OutEp,
                  phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
                  CDC_Handle->DataItf.OutEpSize);

    /* Open channel for IN endpoint */
    USBH_OpenPipe(phost, CDC_Handle->DataItf.InPipe, CDC_Handle->DataItf.InEp,
                  phost->device.address, phost->device.speed, USB_EP_TYPE_BULK,
                  CDC_Handle->DataItf.InEpSize);

    CDC_Handle->state = CDC_IDLE_STATE;

    USBH_LL_SetToggle(phost, CDC_Handle->DataItf.OutPipe, 0U);
    USBH_LL_SetToggle(phost, CDC_Handle->DataItf.InPipe, 0U);

    return USBH_OK;
}


/**
  * @brief  USBH_CDC_InterfaceDeInit
  *         The function DeInit the Pipes used for the CDC class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_CDC_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (CDC_Handle->CommItf.NotifPipe) {
        USBH_ClosePipe(phost, CDC_Handle->CommItf.NotifPipe);
        USBH_FreePipe(phost, CDC_Handle->CommItf.NotifPipe);
        CDC_Handle->CommItf.NotifPipe = 0U;     /* Reset the Channel as Free */
    }

    if (CDC_Handle->DataItf.InPipe) {
        USBH_ClosePipe(phost, CDC_Handle->DataItf.InPipe);
        USBH_FreePipe(phost, CDC_Handle->DataItf.InPipe);
        CDC_Handle->DataItf.InPipe = 0U;     /* Reset the Channel as Free */
    }

    if (CDC_Handle->DataItf.OutPipe) {
        USBH_ClosePipe(phost, CDC_Handle->DataItf.OutPipe);
        USBH_FreePipe(phost, CDC_Handle->DataItf.OutPipe);
        CDC_Handle->DataItf.OutPipe = 0U;    /* Reset the Channel as Free */
    }

    if (phost->pActiveClass->pData) {
        USBH_free(phost->pActiveClass->pData);
        phost->pActiveClass->pData = 0U;
    }

    return USBH_OK;
}

static void CDC_SetInitialValue(USBH_HandleTypeDef *phost)
{
    /*Set the initial value*/
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;
    CDC_Handle->data_rx_state = CDC_IDLE;
    CDC_Handle->data_tx_state = CDC_IDLE;
    CDC_Handle->LineCoding.b.bCharFormat = 0;
    CDC_Handle->LineCoding.b.bDataBits = 8;
    CDC_Handle->LineCoding.b.bParityType = 0;
    CDC_Handle->LineCoding.b.dwDTERate = 460800;
}

/**
  * @brief  USBH_CDC_ClassRequest
  *         The function is responsible for handling Standard requests
  *         for CDC class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_CDC_ClassRequest(USBH_HandleTypeDef *phost)
{
    CDC_SetInitialValue(phost);
    phost->pUser(phost, HOST_USER_CLASS_ACTIVE);

    return USBH_OK;

}

/**
  * @brief  USBH_CDC_Process
  *         The function is for managing state machine for CDC data transfers
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_CDC_Process(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status = USBH_OK;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (CDC_Handle->data_rx_state == CDC_IDLE) {
#if USE_USB_HOST_UART
        USBH_CDC_Receive(phost, cdcBuff, sizeof(cdcBuff));
#endif
    }

    if (CDC_Handle->data_tx_state == CDC_IDLE) {
        uint8_t sendByte = 0;
        uint8_t sendBuff[1024] = {0};
        uint32_t sendLen = 0;
        osDelay(5);

        for (int cnt = 0; cnt < sizeof(sendBuff); cnt++) {
            if (xQueueReceive(ACMDataSendQueue, &sendByte, 0)) {
                sendBuff[sendLen++] = sendByte;
            } else {
                break;
            }
        }
        if (sendLen) {
            USBH_CDC_Transmit(phost, sendBuff, sendLen);
        }
    }
    CDC_ProcessTransmission(phost);
    CDC_ProcessReception(phost);

    return status;
}

/**
  * @brief  USBH_CDC_SOFProcess
  *         The function is for managing SOF callback
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_CDC_SOFProcess(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);

    return USBH_OK;
}


/**
  * @brief  USBH_CDC_Stop
  *         Stop current CDC Transmission
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_CDC_Stop(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        CDC_Handle->state = CDC_IDLE_STATE;

        USBH_ClosePipe(phost, CDC_Handle->CommItf.NotifPipe);
        USBH_ClosePipe(phost, CDC_Handle->DataItf.InPipe);
        USBH_ClosePipe(phost, CDC_Handle->DataItf.OutPipe);
    }
    return USBH_OK;
}

/**
  * @brief  This request allows the host to find out the currently
  *         configured line coding.
  * @param  pdev: Selected device
  * @retval USBH_StatusTypeDef : USB ctl xfer status
  */
static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost, CDC_LineCodingTypeDef *linecoding)
{

    phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_TYPE_CLASS | \
                                         USB_REQ_RECIPIENT_INTERFACE;

    phost->Control.setup.b.bRequest = CDC_GET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}


/**
  * @brief  This request allows the host to specify typical asynchronous
  * line-character formatting properties
  * This request applies to asynchronous byte stream data class interfaces
  * and endpoints
  * @param  pdev: Selected device
  * @retval USBH_StatusTypeDef : USB ctl xfer status
  */
static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost,
                                        CDC_LineCodingTypeDef *linecoding)
{
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS |
                                           USB_REQ_RECIPIENT_INTERFACE;

    phost->Control.setup.b.bRequest = CDC_SET_LINE_CODING;
    phost->Control.setup.b.wValue.w = 0U;

    phost->Control.setup.b.wIndex.w = 0U;

    phost->Control.setup.b.wLength.w = LINE_CODING_STRUCTURE_SIZE;

    return USBH_CtlReq(phost, linecoding->Array, LINE_CODING_STRUCTURE_SIZE);
}

/**
* @brief  This function prepares the state before issuing the class specific commands
* @param  None
* @retval None
*/
USBH_StatusTypeDef USBH_CDC_SetLineCoding(USBH_HandleTypeDef *phost,
                                          CDC_LineCodingTypeDef *linecoding)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        CDC_Handle->state = CDC_SET_LINE_CODING_STATE;
        CDC_Handle->pUserLineCoding = linecoding;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t) USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void) osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, NULL);
#endif
#endif
    }

    return USBH_OK;
}

/**
* @brief  This function prepares the state before issuing the class specific commands
* @param  None
* @retval None
*/
USBH_StatusTypeDef USBH_CDC_GetLineCoding(USBH_HandleTypeDef *phost,
                                          CDC_LineCodingTypeDef *linecoding)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if ((phost->gState == HOST_CLASS) || (phost->gState == HOST_CLASS_REQUEST)) {
        *linecoding = CDC_Handle->LineCoding;
        return USBH_OK;
    } else {
        return USBH_FAIL;
    }
}

/**
  * @brief  This function return last received data size
  * @param  None
  * @retval None
  */
uint16_t USBH_CDC_GetLastReceivedDataSize(USBH_HandleTypeDef *phost)
{
    uint32_t dataSize;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if (phost->gState == HOST_CLASS) {
        dataSize = USBH_LL_GetLastXferSize(phost, CDC_Handle->DataItf.InPipe);
    } else {
        dataSize = 0U;
    }

    return (uint16_t) dataSize;
}

/**
  * @brief  This function prepares the state before issuing the class specific commands
  * @param  None
  * @retval None
  */
USBH_StatusTypeDef USBH_CDC_Transmit(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef Status = USBH_BUSY;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if ((CDC_Handle->state == CDC_IDLE_STATE) || (CDC_Handle->state == CDC_TRANSFER_DATA)) {
        CDC_Handle->pTxData = pbuff;
        CDC_Handle->TxDataLength = length;
        CDC_Handle->state = CDC_TRANSFER_DATA;
        CDC_Handle->data_tx_state = CDC_SEND_DATA;
        Status = USBH_OK;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t) USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void) osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, NULL);
#endif
#endif
    }
    return Status;
}


/**
* @brief  This function prepares the state before issuing the class specific commands
* @param  None
* @retval None
*/
USBH_StatusTypeDef USBH_CDC_Receive(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length)
{
    USBH_StatusTypeDef Status = USBH_BUSY;
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;

    if ((CDC_Handle->state == CDC_IDLE_STATE) || (CDC_Handle->state == CDC_TRANSFER_DATA)) {
        CDC_Handle->pRxData = pbuff;
        CDC_Handle->RxDataLength = length;
        CDC_Handle->state = CDC_TRANSFER_DATA;
        CDC_Handle->data_rx_state = CDC_RECEIVE_DATA;
        Status = USBH_OK;

#if (USBH_USE_OS == 1U)
        phost->os_msg = (uint32_t) USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
        (void) osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
        (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, NULL);
#endif
#endif
    }
    return Status;
}

/**
* @brief  The function is responsible for sending data to the device
*  @param  pdev: Selected device
* @retval None
*/
static void CDC_ProcessTransmission(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;

    switch (CDC_Handle->data_tx_state) {
        case CDC_SEND_DATA:
            if (CDC_Handle->TxDataLength > CDC_Handle->DataItf.OutEpSize) {
                USBH_BulkSendData(phost,
                                  CDC_Handle->pTxData,
                                  CDC_Handle->DataItf.OutEpSize,
                                  CDC_Handle->DataItf.OutPipe,
                                  1U);
            } else {
                USBH_BulkSendData(phost,
                                  CDC_Handle->pTxData,
                                  (uint16_t) CDC_Handle->TxDataLength,
                                  CDC_Handle->DataItf.OutPipe,
                                  1U);
            }

            CDC_Handle->data_tx_state = CDC_SEND_DATA_WAIT;
            break;

        case CDC_SEND_DATA_WAIT:

            URB_Status = USBH_LL_GetURBState(phost, CDC_Handle->DataItf.OutPipe);

            /* Check the status done for transmission */
            if (URB_Status == USBH_URB_DONE) {
                if (CDC_Handle->TxDataLength > CDC_Handle->DataItf.OutEpSize) {
                    CDC_Handle->TxDataLength -= CDC_Handle->DataItf.OutEpSize;
                    CDC_Handle->pTxData += CDC_Handle->DataItf.OutEpSize;
                } else {
                    CDC_Handle->TxDataLength = 0U;
                }

                if (CDC_Handle->TxDataLength > 0U) {
                    CDC_Handle->data_tx_state = CDC_SEND_DATA;
                } else {
                    CDC_Handle->data_tx_state = CDC_IDLE;
                    USBH_CDC_TransmitCallback(phost);
                }

#if (USBH_USE_OS == 1U)
                phost->os_msg = (uint32_t) USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
                (void) osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
                (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, NULL);
#endif
#endif
            } else {
                if (URB_Status == USBH_URB_NOTREADY) {
                    CDC_Handle->data_tx_state = CDC_SEND_DATA;

#if (USBH_USE_OS == 1U)
                    phost->os_msg = (uint32_t) USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
                    (void) osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
                    (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, NULL);
#endif
#endif
                }
            }
            break;

        default:
            break;
    }
}

/**
* @brief  This function responsible for reception of data from the device
*  @param  pdev: Selected device
* @retval None
*/

static void CDC_ProcessReception(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
    uint32_t length;

    switch (CDC_Handle->data_rx_state) {

        case CDC_RECEIVE_DATA:

            USBH_BulkReceiveData(phost,
                                 CDC_Handle->pRxData,
                                 CDC_Handle->DataItf.InEpSize,
                                 CDC_Handle->DataItf.InPipe);

            CDC_Handle->data_rx_state = CDC_RECEIVE_DATA_WAIT;

            break;

        case CDC_RECEIVE_DATA_WAIT:

            URB_Status = USBH_LL_GetURBState(phost, CDC_Handle->DataItf.InPipe);

            /*Check the status done for reception*/
            if (URB_Status == USBH_URB_DONE) {
                length = USBH_LL_GetLastXferSize(phost, CDC_Handle->DataItf.InPipe);

                if (((CDC_Handle->RxDataLength - length) > 0U) && (length > CDC_Handle->DataItf.InEpSize)) {
                    CDC_Handle->RxDataLength -= length;
                    CDC_Handle->pRxData += length;
                    CDC_Handle->data_rx_state = CDC_RECEIVE_DATA;
                } else {
                    CDC_Handle->data_rx_state = CDC_IDLE;
                    USBH_CDC_ReceiveCallback(phost);
                }

#if (USBH_USE_OS == 1U)
                phost->os_msg = (uint32_t) USBH_CLASS_EVENT;
#if (osCMSIS < 0x20000U)
                (void) osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
                (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, NULL);
#endif
#endif
            }
            break;

        default:
            break;
    }
}

/**
* @brief  The function informs user that data have been received
*  @param  pdev: Selected device
* @retval None
*/
__weak void USBH_CDC_TransmitCallback(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

/**
* @brief  The function informs user that data have been sent
*  @param  pdev: Selected device
* @retval None
*/
__weak void USBH_CDC_ReceiveCallback(USBH_HandleTypeDef *phost)
{
    CDC_HandleTypeDef *CDC_Handle = (CDC_HandleTypeDef *) phost->pActiveClass->pData;
    int dataSize = USBH_CDC_GetLastReceivedDataSize(phost);

    for (int i = 0; i < dataSize; i++) {
        xQueueSend(ACMDataRecvQueue, CDC_Handle->pRxData + i, (TickType_t) 0);
    }

    osDelay(1);
}

/**
* @brief  The function informs user that Settings have been changed
*  @param  pdev: Selected device
* @retval None
*/
__weak void USBH_CDC_LineCodingChanged(USBH_HandleTypeDef *phost)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(phost);
}

/**
* @}
*/

/**
* @}
*/

/**
* @}
*/


/**
* @}
*/


/**
* @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
