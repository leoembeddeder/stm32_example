/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#include "uds_iso_tp/uds_flash_fsm.h"

#include <string.h>

static FlashContext_t s_flash_ctx;

void Flash_Init(void) {
    (void)memset(&s_flash_ctx, 0, sizeof(s_flash_ctx));
    s_flash_ctx.state = FLASH_STATE_IDLE;
}

void Flash_SetHardwareInterface(FlashIf_WriteFn write_fn, FlashIf_EraseFn erase_fn) {
    s_flash_ctx.write_fn = write_fn;
    s_flash_ctx.erase_fn = erase_fn;
}

void Flash_ContextClear(void) {
    FlashIf_WriteFn w = s_flash_ctx.write_fn;
    FlashIf_EraseFn e = s_flash_ctx.erase_fn;
    (void)memset(&s_flash_ctx, 0, sizeof(s_flash_ctx));
    s_flash_ctx.state = FLASH_STATE_IDLE;
    s_flash_ctx.write_fn = w;
    s_flash_ctx.erase_fn = e;
}

void Flash_SetWriteInfo(uint32_t memory_addr, uint32_t memory_size) {
    Flash_ContextClear();
    s_flash_ctx.start_addr = memory_addr;
    s_flash_ctx.cur_addr = memory_addr;
    s_flash_ctx.total_size = memory_size;
    s_flash_ctx.written_size = 0U;
}

bool Flash_RequestWrite(const uint8_t *payload, uint16_t length) {
    if ((payload == NULL) || (length == 0U) || (length > FLASH_FSM_CHUNK_SIZE)) {
        return false;
    }
    if ((s_flash_ctx.state != FLASH_STATE_IDLE) && (s_flash_ctx.state != FLASH_STATE_WRITE_DONE)) {
        return false;
    }

    (void)memcpy(s_flash_ctx.write_data, payload, length);
    s_flash_ctx.write_length = length;
    s_flash_ctx.state = FLASH_STATE_WRITE;
    return true;
}

bool Flash_RequestErase(uint32_t addr) {
    if ((s_flash_ctx.state != FLASH_STATE_IDLE) && (s_flash_ctx.state != FLASH_STATE_ERASE_DONE) &&
        (s_flash_ctx.state != FLASH_STATE_WRITE_DONE)) {
        return false;
    }

    s_flash_ctx.erase_addr = addr;
    s_flash_ctx.state = FLASH_STATE_ERASE;
    return true;
}

FlashState_t Flash_GetState(void) {
    return s_flash_ctx.state;
}

bool Flash_IsBusy(void) {
    return (s_flash_ctx.state == FLASH_STATE_WRITE) || (s_flash_ctx.state == FLASH_STATE_ERASE);
}

void Flash_StateClear(void) {
    s_flash_ctx.state = FLASH_STATE_IDLE;
}

void Flash_MainFunction(void) {
    switch (s_flash_ctx.state) {
    case FLASH_STATE_IDLE:
    case FLASH_STATE_WRITE_DONE:
    case FLASH_STATE_WRITE_FAIL:
    case FLASH_STATE_ERASE_DONE:
    case FLASH_STATE_ERASE_FAIL:
        /* Nothing to do in steady states */
        break;

    case FLASH_STATE_WRITE: {
        bool ok = true;
        if (s_flash_ctx.write_fn != NULL) {
            ok = s_flash_ctx.write_fn(s_flash_ctx.cur_addr, s_flash_ctx.write_data,
                                      s_flash_ctx.write_length);
        }
        if (ok) {
            s_flash_ctx.cur_addr += s_flash_ctx.write_length;
            s_flash_ctx.written_size += s_flash_ctx.write_length;
            s_flash_ctx.state = FLASH_STATE_WRITE_DONE;
        } else {
            s_flash_ctx.state = FLASH_STATE_WRITE_FAIL;
        }
        break;
    }

    case FLASH_STATE_ERASE: {
        bool ok = true;
        if (s_flash_ctx.erase_fn != NULL) {
            ok = s_flash_ctx.erase_fn(s_flash_ctx.erase_addr);
        }
        if (ok) {
            s_flash_ctx.state = FLASH_STATE_ERASE_DONE;
        } else {
            s_flash_ctx.state = FLASH_STATE_ERASE_FAIL;
        }
        break;
    }

    default:
        s_flash_ctx.state = FLASH_STATE_IDLE;
        break;
    }
}
