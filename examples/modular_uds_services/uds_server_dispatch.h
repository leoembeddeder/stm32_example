/**
 * @file uds_server_dispatch.h
 * @brief UDS Service Dispatcher with Addressing and Security Masks (ISO 14229-1, Issue #113)
 */

#ifndef UDS_SERVER_DISPATCH_H
#define UDS_SERVER_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>
#include "uds_types.h"

/* Addressing Modes (Issue #113) */
#define UDS_ADDR_PHYSICAL     (1U << 0)
#define UDS_ADDR_FUNCTIONAL   (1U << 1)
#define UDS_ADDR_ALL          (UDS_ADDR_PHYSICAL | UDS_ADDR_FUNCTIONAL)

/* Security Levels (Issue #113) */
#define UDS_SEC_LEVEL_LOCKED  (1U << 0)
#define UDS_SEC_LEVEL_1       (1U << 1)
#define UDS_SEC_LEVEL_2       (1U << 2)
#define UDS_SEC_LEVEL_ALL     0xFFFFFFFFU

#define UDS_MAX_SERVICES_EXT  32U

typedef struct {
    uint8_t               sid;
    uds_service_handler_t handler;
    uint8_t               session_mask;  /**< Allowed sessions bitmask */
    uint32_t              addressMask;   /**< UDS_ADDR_PHYSICAL / UDS_ADDR_FUNCTIONAL */
    uint32_t              securityMask;  /**< Required security levels bitmask */
} uds_service_entry_ext_t;

void uds_server_dispatch_init(void);

bool uds_server_dispatch_register(uint8_t sid, uds_service_handler_t handler,
                                  uint8_t session_mask, uint32_t address_mask, uint32_t security_mask);

void uds_server_dispatch_set_security_level(uint32_t active_level_mask);

uint32_t uds_server_dispatch_get_security_level(void);

void uds_server_dispatch_set_session(uint8_t session_mask);

uint8_t uds_server_dispatch_get_session(void);

bool uds_server_dispatch_process(const uint8_t *data, uint16_t len, bool functional, uds_response_t *resp);

/**
 * @brief Refresh active session S3 timeout timer (ISO 14229-1 S3server)
 */
void uds_session_refresh(void);

#endif /* UDS_SERVER_DISPATCH_H */
