#include "can_transport.h"

#include <stddef.h>

void uds_can_transport_init(UdsCanTransport *transport, CAN_HandleTypeDef *hcan,
                            uint32_t request_id, uint32_t response_id) {
    if (transport == NULL)
        return;
    transport->hcan = hcan;
    transport->request_id = request_id;
    transport->response_id = response_id;
    transport->tx_mailbox_mask = 0U;
    transport->tx_error_occurred = false;
}

bool uds_can_transport_send(void *context, const IsoTpCanFrame *frame) {
    UdsCanTransport *transport = (UdsCanTransport *)context;
    if ((transport == NULL) || (transport->hcan == NULL) || (frame == NULL) || frame->is_fd ||
        (frame->dlc > 8U))
        return false;

    CAN_TxHeaderTypeDef header = {0};
    header.StdId = frame->can_id;
    header.ExtId = 0U;
    header.RTR = CAN_RTR_DATA;
    header.IDE = CAN_ID_STD;
    header.DLC = frame->dlc;
    header.TransmitGlobalTime = DISABLE;
    uint32_t mailbox = 0U;
    if (HAL_CAN_AddTxMessage(transport->hcan, &header, (uint8_t *)frame->data, &mailbox) !=
        HAL_OK) {
        transport->tx_error_occurred = true;
        return false;
    }
    transport->tx_mailbox_mask |= mailbox;
    return true;
}

bool uds_can_transport_tx_complete(void *context) {
    UdsCanTransport *transport = (UdsCanTransport *)context;
    if ((transport == NULL) || (transport->hcan == NULL))
        return false;
    if (transport->tx_error_occurred)
        return false;
    if (transport->tx_mailbox_mask == 0U)
        return true;
    if (HAL_CAN_IsTxMessagePending(transport->hcan, transport->tx_mailbox_mask) != 0U)
        return false;
    transport->tx_mailbox_mask = 0U;
    return true;
}

bool uds_can_transport_tx_error(void *context) {
    UdsCanTransport *transport = (UdsCanTransport *)context;
    if ((transport == NULL) || (transport->hcan == NULL))
        return false;

    if (transport->tx_error_occurred) {
        transport->tx_error_occurred = false;
        if (transport->tx_mailbox_mask != 0U) {
            (void)HAL_CAN_AbortTxRequest(transport->hcan, transport->tx_mailbox_mask);
            transport->tx_mailbox_mask = 0U;
        }
        return true;
    }

    uint32_t err = HAL_CAN_GetError(transport->hcan);
    bool bus_off = (err & HAL_CAN_ERROR_BOF) != 0U;
    if ((transport->hcan->Instance != NULL) &&
        ((transport->hcan->Instance->ESR & CAN_ESR_BOFF) != 0U)) {
        bus_off = true;
    }
    if (bus_off) {
        if (transport->tx_mailbox_mask != 0U) {
            (void)HAL_CAN_AbortTxRequest(transport->hcan, transport->tx_mailbox_mask);
            transport->tx_mailbox_mask = 0U;
        }
        return true;
    }

    if (transport->tx_mailbox_mask != 0U) {
        uint32_t tx_err_mask = 0U;
        if ((transport->tx_mailbox_mask & CAN_TX_MAILBOX0) != 0U) {
            tx_err_mask |= (HAL_CAN_ERROR_TX_TERR0 | HAL_CAN_ERROR_TX_ALST0);
        }
        if ((transport->tx_mailbox_mask & CAN_TX_MAILBOX1) != 0U) {
            tx_err_mask |= (HAL_CAN_ERROR_TX_TERR1 | HAL_CAN_ERROR_TX_ALST1);
        }
        if ((transport->tx_mailbox_mask & CAN_TX_MAILBOX2) != 0U) {
            tx_err_mask |= (HAL_CAN_ERROR_TX_TERR2 | HAL_CAN_ERROR_TX_ALST2);
        }
        if ((err & tx_err_mask) != 0U) {
            (void)HAL_CAN_AbortTxRequest(transport->hcan, transport->tx_mailbox_mask);
            transport->tx_mailbox_mask = 0U;
            return true;
        }
    }
    return false;
}

bool uds_can_transport_recover(UdsCanTransport *transport) {
    if ((transport == NULL) || (transport->hcan == NULL))
        return false;
    if (transport->tx_mailbox_mask != 0U) {
        (void)HAL_CAN_AbortTxRequest(transport->hcan, transport->tx_mailbox_mask);
        transport->tx_mailbox_mask = 0U;
    }
    transport->tx_error_occurred = false;
    (void)HAL_CAN_Stop(transport->hcan);
    return (HAL_CAN_Start(transport->hcan) == HAL_OK);
}

uint32_t uds_can_transport_clock(void *context) {
    (void)context;
    return HAL_GetTick();
}
