/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#include "uds_iso_tp/uds_flash_fsm.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MOCK_MEM_SIZE 1024U
static uint8_t s_mock_mem[MOCK_MEM_SIZE];
static bool s_mock_fail_write = false;
static bool s_mock_fail_erase = false;

static bool mock_write(uint32_t addr, const uint8_t *data, uint16_t len) {
    if (s_mock_fail_write) {
        return false;
    }
    if ((addr + len) > MOCK_MEM_SIZE) {
        return false;
    }
    (void)memcpy(&s_mock_mem[addr], data, len);
    return true;
}

static bool mock_erase(uint32_t addr) {
    if (s_mock_fail_erase) {
        return false;
    }
    if (addr >= MOCK_MEM_SIZE) {
        return false;
    }
    (void)memset(&s_mock_mem[addr], 0xFF, 128U);
    return true;
}

int main(void) {
    (void)memset(s_mock_mem, 0, sizeof(s_mock_mem));

    Flash_Init();
    assert(Flash_GetState() == FLASH_STATE_IDLE);
    assert(!Flash_IsBusy());

    Flash_SetHardwareInterface(mock_write, mock_erase);

    /* 1. Test invalid arguments */
    assert(!Flash_RequestWrite(NULL, 10));
    assert(!Flash_RequestWrite((const uint8_t *)"abc", 0));
    assert(!Flash_RequestWrite((const uint8_t *)"abc", FLASH_FSM_CHUNK_SIZE + 1U));

    /* 2. Test successful chunk write */
    Flash_SetWriteInfo(0x100U, 64U);
    const uint8_t sample_data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    assert(Flash_RequestWrite(sample_data, 8));
    assert(Flash_GetState() == FLASH_STATE_WRITE);
    assert(Flash_IsBusy());

    /* While busy, reject new write */
    assert(!Flash_RequestWrite(sample_data, 8));

    /* Process state machine */
    Flash_MainFunction();
    assert(Flash_GetState() == FLASH_STATE_WRITE_DONE);
    assert(!Flash_IsBusy());
    assert(memcmp(&s_mock_mem[0x100U], sample_data, 8) == 0);

    /* 3. Test erase */
    Flash_StateClear();
    assert(Flash_GetState() == FLASH_STATE_IDLE);
    assert(Flash_RequestErase(0x100U));
    assert(Flash_GetState() == FLASH_STATE_ERASE);
    assert(Flash_IsBusy());

    Flash_MainFunction();
    assert(Flash_GetState() == FLASH_STATE_ERASE_DONE);
    assert(!Flash_IsBusy());
    assert(s_mock_mem[0x100U] == 0xFFU);
    assert(s_mock_mem[0x107U] == 0xFFU);

    /* 4. Test write failure handling */
    Flash_StateClear();
    s_mock_fail_write = true;
    assert(Flash_RequestWrite(sample_data, 8));
    Flash_MainFunction();
    assert(Flash_GetState() == FLASH_STATE_WRITE_FAIL);
    s_mock_fail_write = false;

    /* 5. Test erase failure handling */
    Flash_StateClear();
    s_mock_fail_erase = true;
    assert(Flash_RequestErase(0x200U));
    Flash_MainFunction();
    assert(Flash_GetState() == FLASH_STATE_ERASE_FAIL);
    s_mock_fail_erase = false;

    /* 6. Test context clear */
    Flash_ContextClear();
    assert(Flash_GetState() == FLASH_STATE_IDLE);

    return 0;
}
