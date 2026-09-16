/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#ifndef STM32_UDS_ISO_TP_UDS_MEMORY_H
#define STM32_UDS_ISO_TP_UDS_MEMORY_H

#include "uds_iso_tp/uds.h"
#include "uds_iso_tp/uds_services.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDS_MEMORY_FLAG_READ (1U << 0U)
#define UDS_MEMORY_FLAG_WRITE (1U << 1U)
#define UDS_MEMORY_FLAG_SECURE_READ (1U << 2U)
#define UDS_MEMORY_FLAG_SECURE_WRITE (1U << 3U)
#define UDS_MEMORY_FLAG_PROG_ONLY (1U << 4U)

typedef UdsCallbackResult (*UdsMemoryReadDriverFn)(void *driver_ctx, uint32_t address,
                                                   uint8_t *data, uint32_t length);

typedef UdsCallbackResult (*UdsMemoryWriteDriverFn)(void *driver_ctx, uint32_t address,
                                                    const uint8_t *data, uint32_t length);

typedef struct {
    uint32_t start_address;
    uint32_t size;
    uint32_t flags;
    UdsMemoryReadDriverFn read;
    UdsMemoryWriteDriverFn write;
    void *driver_ctx;
} UdsMemoryRegion;

typedef struct {
    const UdsMemoryRegion *regions;
    size_t region_count;
    uint32_t max_read_size;
    uint32_t max_write_size;
    bool (*condition_check)(void *user_ctx, uint8_t sid, uint32_t address, uint32_t size);
    void *user_ctx;
    const UdsServer *server;
} UdsMemoryConfig;

typedef struct {
    UdsMemoryConfig config;
    UdsMemoryServiceBackend backend;
} UdsMemoryManager;

bool uds_memory_decode_alfid(uint8_t alfid, uint8_t *addr_len, uint8_t *size_len);
bool uds_memory_decode_u32(const uint8_t *buf, uint8_t len, uint32_t *val);

void uds_memory_init(UdsMemoryManager *mgr, const UdsMemoryConfig *config);
void uds_memory_set_server(UdsMemoryManager *mgr, const UdsServer *server);
const UdsMemoryServiceBackend *uds_memory_get_backend(const UdsMemoryManager *mgr);

UdsCallbackResult uds_memory_read_handler(void *context, const uint8_t *request,
                                          uint16_t request_len, uint8_t *response,
                                          uint16_t *response_len, uint16_t capacity);

UdsCallbackResult uds_memory_write_handler(void *context, const uint8_t *request,
                                           uint16_t request_len, uint8_t *response,
                                           uint16_t *response_len, uint16_t capacity);

UdsCallbackResult uds_memory_check_access(void *context, uint8_t sid, const uint8_t *request,
                                          uint16_t request_len);

#ifdef __cplusplus
}
#endif

#endif /* STM32_UDS_ISO_TP_UDS_MEMORY_H */
