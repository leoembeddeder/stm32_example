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

#include "uds_iso_tp/uds.h"

#define RANGE_ERASE_TOTAL_SIZE 131072U
#define RANGE_ERASE_SECTOR_SIZE 8192U
static uint8_t s_128k_mem[RANGE_ERASE_TOTAL_SIZE];
static uint32_t s_sector_erase_count = 0U;

static bool mock_range_erase_sector(uint32_t addr) {
    if ((addr < 0x08020000U) || (addr >= (0x08020000U + RANGE_ERASE_TOTAL_SIZE))) {
        return false;
    }
    uint32_t offset = addr - 0x08020000U;
    (void)memset(&s_128k_mem[offset], 0xFF, RANGE_ERASE_SECTOR_SIZE);
    s_sector_erase_count++;
    return true;
}

static void test_128kib_range_erase_interleaved_uds_service(void) {
    (void)memset(s_128k_mem, 0xAA, sizeof(s_128k_mem));
    s_sector_erase_count = 0U;

    Flash_Init();
    Flash_SetHardwareInterface(mock_write, mock_range_erase_sector);

    /* Setup UDS server */
    UdsServer server;
    UdsCallbacks callbacks;
    (void)memset(&callbacks, 0, sizeof(callbacks));
    uds_server_init(&server, &callbacks, NULL, 0U);

    /* Request 128 KiB range erase sliced into 16 sectors of 8 KiB */
    assert(Flash_RequestRangeErase(0x08020000U, RANGE_ERASE_TOTAL_SIZE, RANGE_ERASE_SECTOR_SIZE));
    assert(Flash_IsBusy());
    assert(Flash_GetState() == FLASH_STATE_ERASE);

    uint32_t service_ticks = 0U;
    uint32_t successful_uds_responses = 0U;

    while (Flash_IsBusy()) {
        /* Run one non-blocking erase slice */
        Flash_MainFunction();
        service_ticks++;

        /* Interleaved: simulate UDS server servicing ISO-TP flow control and requests */
        (void)uds_server_tick(&server, service_ticks * 10U);

        /* Send TesterPresent (0x3E 0x00) request during erase flight */
        const uint8_t tp_req[2] = {0x3E, 0x00};
        uint8_t resp_buf[16];
        uint16_t resp_len = 0U;
        UdsCallbackResult res =
            uds_server_handle(&server, tp_req, (uint16_t)sizeof(tp_req), resp_buf, &resp_len,
                              (uint16_t)sizeof(resp_buf), service_ticks * 10U);
        if ((res == UDS_RESULT_OK) && (resp_len == 2U) && (resp_buf[0] == 0x7EU) &&
            (resp_buf[1] == 0x00U)) {
            successful_uds_responses++;
        }
    }

    assert(Flash_GetState() == FLASH_STATE_ERASE_DONE);
    assert(!Flash_IsBusy());
    assert(service_ticks == 16U);
    assert(s_sector_erase_count == 16U);
    assert(successful_uds_responses == 16U);

    /* Verify all 128 KiB memory was erased to 0xFF */
    for (size_t i = 0U; i < RANGE_ERASE_TOTAL_SIZE; ++i) {
        assert(s_128k_mem[i] == 0xFFU);
    }
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

    /* 7. Test 128 KiB range erase with interleaved UDS server servicing */
    test_128kib_range_erase_interleaved_uds_service();

    return 0;
}
