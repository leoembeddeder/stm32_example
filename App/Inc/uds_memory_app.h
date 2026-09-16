/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef APP_UDS_MEMORY_APP_H
#define APP_UDS_MEMORY_APP_H

#include "uds_iso_tp/uds_memory.h"
#include "uds_iso_tp/uds_services.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UDS_MEMORY_APP_CALIBRATION_ADDR 0x20004000U
#define UDS_MEMORY_APP_CALIBRATION_SIZE 0x00001000U /* 4 KB */

#define UDS_MEMORY_APP_NVM_CONFIG_ADDR 0x20005000U
#define UDS_MEMORY_APP_NVM_CONFIG_SIZE 0x00000400U /* 1 KB */

#define UDS_MEMORY_APP_FLASH_APP_ADDR 0x08040000U
#define UDS_MEMORY_APP_FLASH_APP_SIZE 0x00040000U /* 256 KB */

void uds_memory_app_init(void);
void uds_memory_app_set_server(const UdsServer *server);
const UdsMemoryServiceBackend *uds_memory_app_get_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UDS_MEMORY_APP_H */
