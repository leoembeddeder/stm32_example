/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#include "uds_iso_tp/uds_wear_leveling.h"

#include <string.h>

#define UDS_CRC16_CCITT_INIT 0xFFFFu
#define UDS_CRC16_CCITT_POLY 0x1021u

uint16_t uds_crc16_ccitt_update(uint16_t crc, const void *data, size_t len) {
    if (data == NULL) {
        return crc;
    }
    const uint8_t *ptr = (const uint8_t *)data;
    uint16_t val = crc;
    for (size_t i = 0U; i < len; ++i) {
        val ^= (uint16_t)((uint16_t)ptr[i] << 8U);
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            if ((val & 0x8000U) != 0U) {
                val = (uint16_t)((val << 1U) ^ UDS_CRC16_CCITT_POLY);
            } else {
                val = (uint16_t)(val << 1U);
            }
        }
    }
    return val;
}

uint16_t uds_crc16_ccitt(const void *data, size_t len) {
    return uds_crc16_ccitt_update(UDS_CRC16_CCITT_INIT, data, len);
}

static uint16_t align16(uint16_t size) {
    return (uint16_t)((size + 15U) & (uint16_t)~15U);
}

static uint32_t slot_addr(const UdsParamStore *store, uint8_t sector, uint16_t slot) {
    return store->flash_base + ((uint32_t)sector * store->sector_size) +
           ((uint32_t)slot * store->slot_size);
}

static bool verify_slot_crc(const UdsParamStore *store, uint8_t sector, uint16_t slot,
                            uint16_t expected_crc) {
    uint32_t addr = slot_addr(store, sector, slot) + (uint32_t)sizeof(UdsParamSlotHeader);
    uint16_t crc = UDS_CRC16_CCITT_INIT;
    uint16_t remaining = store->data_size;
    uint8_t chunk[32];

    while (remaining > 0U) {
        uint16_t chunk_len =
            (remaining > (uint16_t)sizeof(chunk)) ? (uint16_t)sizeof(chunk) : remaining;
        if (store->port->read(addr, chunk, chunk_len) != 0) {
            return false;
        }
        crc = uds_crc16_ccitt_update(crc, chunk, chunk_len);
        addr += chunk_len;
        remaining = (uint16_t)(remaining - chunk_len);
    }
    return (crc == expected_crc);
}

static bool read_slot_header(const UdsParamStore *store, uint8_t sector, uint16_t slot,
                             UdsParamSlotHeader *hdr) {
    uint32_t addr = slot_addr(store, sector, slot);
    if (store->port->read(addr, hdr, sizeof(UdsParamSlotHeader)) != 0) {
        return false;
    }
    if ((hdr->magic != UDS_PARAM_MAGIC) || (hdr->data_size != store->data_size)) {
        return false;
    }
    return verify_slot_crc(store, sector, slot, hdr->crc);
}

int uds_param_init(UdsParamStore *store, const UdsFlashPort *port, uint32_t flash_base,
                   uint8_t sector_count, uint16_t data_size) {
    if ((store == NULL) || (port == NULL) || (port->erase == NULL) || (port->read == NULL) ||
        (port->write == NULL) || (port->sector_size == NULL) || (sector_count == 0U) ||
        (data_size == 0U)) {
        return UDS_PARAM_INVALID_PARAM;
    }

    uint32_t sec_size = port->sector_size(flash_base);
    if (sec_size == 0U) {
        return UDS_PARAM_INVALID_PARAM;
    }

    uint16_t raw_slot = (uint16_t)(sizeof(UdsParamSlotHeader) + data_size);
    uint16_t aligned_slot = align16(raw_slot);
    if ((uint32_t)aligned_slot > sec_size) {
        return UDS_PARAM_INVALID_PARAM;
    }

    store->port = port;
    store->flash_base = flash_base;
    store->sector_size = sec_size;
    store->sector_count = sector_count;
    store->data_size = data_size;
    store->slot_size = aligned_slot;
    store->slots_per_sector = (uint16_t)(sec_size / aligned_slot);
    store->active_sector = 0U;
    store->active_slot = 0U;
    store->next_seq = 1U;
    store->initialized = false;
    store->has_active_slot = false;

    /* Scan all sectors and slots for the latest valid entry */
    uint16_t best_seq = 0U;
    uint8_t best_sec = 0U;
    uint16_t best_slot = 0U;
    bool found_valid = false;

    for (uint8_t sec = 0U; sec < sector_count; ++sec) {
        for (uint16_t sl = 0U; sl < store->slots_per_sector; ++sl) {
            UdsParamSlotHeader hdr;
            if (read_slot_header(store, sec, sl, &hdr)) {
                if (!found_valid || (hdr.seq > best_seq)) {
                    best_seq = hdr.seq;
                    best_sec = sec;
                    best_slot = sl;
                    found_valid = true;
                }
            }
        }
    }

    if (found_valid) {
        store->active_sector = best_sec;
        store->active_slot = best_slot;
        store->next_seq = (uint16_t)(best_seq + 1U);
        store->has_active_slot = true;
    }

    store->initialized = true;
    return UDS_PARAM_OK;
}

int uds_param_load(const UdsParamStore *store, void *data) {
    if ((store == NULL) || (!store->initialized) || (data == NULL)) {
        return UDS_PARAM_INVALID_PARAM;
    }
    if (!store->has_active_slot) {
        return UDS_PARAM_ERR;
    }

    uint32_t addr = slot_addr(store, store->active_sector, store->active_slot) +
                    (uint32_t)sizeof(UdsParamSlotHeader);
    if (store->port->read(addr, data, store->data_size) != 0) {
        return UDS_PARAM_ERR;
    }

    uint16_t crc = uds_crc16_ccitt(data, store->data_size);
    UdsParamSlotHeader hdr;
    uint32_t hdr_addr = slot_addr(store, store->active_sector, store->active_slot);
    if (store->port->read(hdr_addr, &hdr, sizeof(hdr)) != 0) {
        return UDS_PARAM_ERR;
    }
    if (hdr.crc != crc) {
        return UDS_PARAM_ERR;
    }

    return UDS_PARAM_OK;
}

int uds_param_save(UdsParamStore *store, const void *data) {
    if ((store == NULL) || (!store->initialized) || (data == NULL)) {
        return UDS_PARAM_INVALID_PARAM;
    }

    uint8_t sec = 0U;
    uint16_t sl = 0U;

    if (store->has_active_slot) {
        sl = (uint16_t)(store->active_slot + 1U);
        sec = store->active_sector;
        if (sl >= store->slots_per_sector) {
            sec = (uint8_t)((sec + 1U) % store->sector_count);
            sl = 0U;
            if (store->port->erase(store->flash_base + ((uint32_t)sec * store->sector_size)) != 0) {
                return UDS_PARAM_ERR;
            }
        }
    } else {
        sec = 0U;
        sl = 0U;
        if (store->port->erase(store->flash_base) != 0) {
            return UDS_PARAM_ERR;
        }
    }

    UdsParamSlotHeader hdr;
    memset(&hdr, 0xFF, sizeof(hdr));
    hdr.magic = UDS_PARAM_MAGIC;
    hdr.seq = store->next_seq;
    hdr.data_size = store->data_size;
    hdr.crc = uds_crc16_ccitt(data, store->data_size);

    uint32_t addr = slot_addr(store, sec, sl);

    /* 1. Write payload data first */
    if (store->port->write(addr + (uint32_t)sizeof(hdr), data, store->data_size) != 0) {
        return UDS_PARAM_ERR;
    }

    /* 2. Write header with magic number last (atomic commit) */
    if (store->port->write(addr, &hdr, sizeof(hdr)) != 0) {
        return UDS_PARAM_ERR;
    }

    store->active_sector = sec;
    store->active_slot = sl;
    store->next_seq++;
    store->has_active_slot = true;

    return UDS_PARAM_OK;
}

int uds_param_erase_all(UdsParamStore *store) {
    if ((store == NULL) || (!store->initialized)) {
        return UDS_PARAM_INVALID_PARAM;
    }

    for (uint8_t sec = 0U; sec < store->sector_count; ++sec) {
        uint32_t addr = store->flash_base + ((uint32_t)sec * store->sector_size);
        if (store->port->erase(addr) != 0) {
            return UDS_PARAM_ERR;
        }
    }

    store->active_sector = 0U;
    store->active_slot = 0U;
    store->next_seq = 1U;
    store->has_active_slot = false;

    return UDS_PARAM_OK;
}

uint16_t uds_param_write_count(const UdsParamStore *store) {
    if ((store == NULL) || (!store->has_active_slot) || (store->next_seq == 0U)) {
        return 0U;
    }
    return (uint16_t)(store->next_seq - 1U);
}
