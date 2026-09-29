/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */

#include "uds_iso_tp/uds_wear_leveling.h"
#include "uds_dtc_app.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TEST_SECTOR_SIZE 128U
#define TEST_SECTOR_COUNT 3U
static uint8_t s_ram_flash[TEST_SECTOR_SIZE * TEST_SECTOR_COUNT];

static int ram_flash_erase(uint32_t addr) {
    uint32_t sec = addr / TEST_SECTOR_SIZE;
    if (sec >= TEST_SECTOR_COUNT) {
        return -1;
    }
    memset(&s_ram_flash[sec * TEST_SECTOR_SIZE], 0xFF, TEST_SECTOR_SIZE);
    return 0;
}

static int ram_flash_read(uint32_t addr, void *buf, size_t len) {
    if ((addr + len) > sizeof(s_ram_flash)) {
        return -1;
    }
    memcpy(buf, &s_ram_flash[addr], len);
    return 0;
}

static int ram_flash_write(uint32_t addr, const void *buf, size_t len) {
    if ((addr + len) > sizeof(s_ram_flash)) {
        return -1;
    }
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0U; i < len; ++i) {
        /* NOR flash bit-clearing simulation */
        s_ram_flash[addr + i] &= src[i];
    }
    return 0;
}

static uint32_t ram_flash_sector_size(uint32_t addr) {
    (void)addr;
    return TEST_SECTOR_SIZE;
}

static const UdsFlashPort s_test_flash_port = {
    .erase = ram_flash_erase,
    .read = ram_flash_read,
    .write = ram_flash_write,
    .sector_size = ram_flash_sector_size,
};

typedef struct {
    uint16_t sensor_val;
    uint8_t flags;
    uint8_t mode;
} TestData;

static void test_crc16(void) {
    const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39}; /* "123456789" */
    uint16_t crc = uds_crc16_ccitt(data, sizeof(data));
    assert(crc == 0x29B1U); /* Standard CRC-16 CCITT for 123456789 with 0xFFFF init is 0x29B1 */
}

static void test_basic_wear_leveling(void) {
    memset(s_ram_flash, 0xFF, sizeof(s_ram_flash));
    UdsParamStore store;
    int res = uds_param_init(&store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT, sizeof(TestData));
    assert(res == UDS_PARAM_OK);
    assert(!store.has_active_slot);
    assert(uds_param_write_count(&store) == 0U);

    TestData write_data = {.sensor_val = 0x1234U, .flags = 0x55U, .mode = 0x02U};
    res = uds_param_save(&store, &write_data);
    assert(res == UDS_PARAM_OK);
    assert(store.has_active_slot);
    assert(uds_param_write_count(&store) == 1U);
    assert(store.active_sector == 0U);
    assert(store.active_slot == 0U);

    TestData read_data;
    memset(&read_data, 0, sizeof(read_data));
    res = uds_param_load(&store, &read_data);
    assert(res == UDS_PARAM_OK);
    assert(read_data.sensor_val == 0x1234U);
    assert(read_data.flags == 0x55U);
    assert(read_data.mode == 0x02U);
}

static void test_sector_rotation(void) {
    memset(s_ram_flash, 0xFF, sizeof(s_ram_flash));
    UdsParamStore store;
    uds_param_init(&store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT, sizeof(TestData));

    /* Slot size for TestData: sizeof(hdr)(16) + 4 = 20, aligned to 16 = 32 bytes.
     * Sector size = 128 bytes -> 4 slots per sector!
     */
    assert(store.slots_per_sector == 4U);

    for (uint16_t i = 1U; i <= 10U; ++i) {
        TestData td = {.sensor_val = i, .flags = (uint8_t)i, .mode = (uint8_t)(i * 2U)};
        assert(uds_param_save(&store, &td) == UDS_PARAM_OK);
        assert(uds_param_write_count(&store) == i);
    }

    /* 10 saves:
     * Saves 1..4 in Sector 0 (slots 0..3)
     * Saves 5..8 in Sector 1 (slots 0..3)
     * Saves 9..10 in Sector 2 (slots 0..1)
     */
    assert(store.active_sector == 2U);
    assert(store.active_slot == 1U);

    TestData loaded;
    assert(uds_param_load(&store, &loaded) == UDS_PARAM_OK);
    assert(loaded.sensor_val == 10U);

    /* Simulate reboot by re-initializing another store instance over the same flash */
    UdsParamStore store2;
    assert(uds_param_init(&store2, &s_test_flash_port, 0U, TEST_SECTOR_COUNT, sizeof(TestData)) ==
           UDS_PARAM_OK);
    assert(store2.has_active_slot);
    assert(store2.active_sector == 2U);
    assert(store2.active_slot == 1U);
    assert(uds_param_write_count(&store2) == 10U);

    memset(&loaded, 0, sizeof(loaded));
    assert(uds_param_load(&store2, &loaded) == UDS_PARAM_OK);
    assert(loaded.sensor_val == 10U);
}

static void test_power_loss_recovery(void) {
    memset(s_ram_flash, 0xFF, sizeof(s_ram_flash));
    UdsParamStore store;
    uds_param_init(&store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT, sizeof(TestData));

    TestData valid = {.sensor_val = 0xAAAAU, .flags = 0x01U, .mode = 0x01U};
    assert(uds_param_save(&store, &valid) == UDS_PARAM_OK);

    /* Simulate interrupted / torn write in slot 1: corrupt header without valid CRC */
    uint32_t torn_slot_addr = store.slot_size;
    UdsParamSlotHeader torn_hdr = {
        .magic = UDS_PARAM_MAGIC,
        .seq = 999U,
        .data_size = sizeof(TestData),
        .crc = 0xDEADU, /* Deliberately corrupted CRC */
    };
    s_test_flash_port.write(torn_slot_addr, &torn_hdr, sizeof(torn_hdr));

    /* Re-init: should detect CRC corruption and fall back to slot 0 */
    UdsParamStore reboot_store;
    assert(uds_param_init(&reboot_store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT,
                          sizeof(TestData)) == UDS_PARAM_OK);
    assert(reboot_store.has_active_slot);
    assert(reboot_store.active_slot == 0U);
    assert(uds_param_write_count(&reboot_store) == 1U);

    TestData recovered;
    assert(uds_param_load(&reboot_store, &recovered) == UDS_PARAM_OK);
    assert(recovered.sensor_val == 0xAAAAU);
}

#define DTC_SECTOR_SIZE 2048U
#define DTC_SECTOR_COUNT 2U
static uint8_t s_dtc_flash[DTC_SECTOR_SIZE * DTC_SECTOR_COUNT];

static int dtc_flash_erase(uint32_t addr) {
    uint32_t sec = addr / DTC_SECTOR_SIZE;
    if (sec >= DTC_SECTOR_COUNT) {
        return -1;
    }
    memset(&s_dtc_flash[sec * DTC_SECTOR_SIZE], 0xFF, DTC_SECTOR_SIZE);
    return 0;
}

static int dtc_flash_read(uint32_t addr, void *buf, size_t len) {
    if ((addr + len) > sizeof(s_dtc_flash)) {
        return -1;
    }
    memcpy(buf, &s_dtc_flash[addr], len);
    return 0;
}

static int dtc_flash_write(uint32_t addr, const void *buf, size_t len) {
    if ((addr + len) > sizeof(s_dtc_flash)) {
        return -1;
    }
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0U; i < len; ++i) {
        s_dtc_flash[addr + i] &= src[i];
    }
    return 0;
}

static uint32_t dtc_flash_sector_size(uint32_t addr) {
    (void)addr;
    return DTC_SECTOR_SIZE;
}

static const UdsFlashPort s_dtc_flash_port = {
    .erase = dtc_flash_erase,
    .read = dtc_flash_read,
    .write = dtc_flash_write,
    .sector_size = dtc_flash_sector_size,
};

static void test_dtc_persistence_integration(void) {
    memset(s_dtc_flash, 0xFF, sizeof(s_dtc_flash));
    UdsParamStore dtc_nvm;
    assert(uds_param_init(&dtc_nvm, &s_dtc_flash_port, 0U, DTC_SECTOR_COUNT,
                          sizeof(UdsDtcNvBlock)) == UDS_PARAM_OK);

    uds_dtc_app_init();
    uds_dtc_app_attach_nvm(&dtc_nvm);

    /* Update a DTC status and verify it persists */
    uds_dtc_app_set_fault(0xD00616U, UDS_DTC_STATUS_CONFIRMED, 0x02U, 50);
    assert(dtc_nvm.has_active_slot);
    assert(uds_param_write_count(&dtc_nvm) >= 1U);

    /* Simulate ECU power cycle by re-initializing DTC app and restoring from NVM */
    uds_dtc_app_init();
    uds_dtc_app_attach_nvm(&dtc_nvm);
    bool loaded = uds_dtc_app_load_from_nvm();
    assert(loaded);

    /* Verify status query via backend returns the restored DTC */
    const UdsDtcBackend *backend = uds_dtc_app_get_backend();
    uint8_t req[] = {0x19U, 0x02U, 0xFFU};
    uint8_t resp[64];
    uint16_t resp_len = 0U;
    UdsCallbackResult res =
        backend->report(NULL, 0x02U, req, sizeof(req), resp, &resp_len, sizeof(resp));
    assert(res == UDS_RESULT_OK);
    assert(resp_len >= 6U);

    /* Clear via 0x14 and verify persisted to NVM */
    assert(uds_dtc_app_clear(NULL, 0xFFFFFFUL) == UDS_RESULT_OK);

    /* Re-init again and verify cleared state persisted */
    uds_dtc_app_init();
    uds_dtc_app_attach_nvm(&dtc_nvm);
    assert(uds_dtc_app_load_from_nvm());
}

static void test_sequence_rollover(void) {
    memset(s_ram_flash, 0xFF, sizeof(s_ram_flash));
    UdsParamStore store;
    assert(uds_param_init(&store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT, sizeof(TestData)) ==
           UDS_PARAM_OK);

    /* Force sequence number near rollover */
    store.next_seq = 65535U;
    TestData d1 = {.sensor_val = 0x1111U, .flags = 1U, .mode = 1U};
    assert(uds_param_save(&store, &d1) == UDS_PARAM_OK);
    assert(store.next_seq == 1U); /* Rollover should skip 0 */

    TestData d2 = {.sensor_val = 0x2222U, .flags = 2U, .mode = 2U};
    assert(uds_param_save(&store, &d2) == UDS_PARAM_OK);

    /* Reboot simulation: re-init store and ensure d2 (seq 1) is selected over d1 (seq 65535) */
    UdsParamStore reboot_store;
    assert(uds_param_init(&reboot_store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT,
                          sizeof(TestData)) == UDS_PARAM_OK);
    assert(reboot_store.has_active_slot);

    TestData recovered;
    assert(uds_param_load(&reboot_store, &recovered) == UDS_PARAM_OK);
    assert(recovered.sensor_val == 0x2222U);
    assert(recovered.flags == 2U);
}

#define STRICT_SECTOR_SIZE 512U
#define STRICT_SECTOR_COUNT 3U

typedef struct {
    uint8_t buffer[STRICT_SECTOR_SIZE * STRICT_SECTOR_COUNT];
    uint8_t granule;
    uint8_t erased_byte;
    int fail_after_write_count;
    int write_count;
} StrictFlashSim;

static StrictFlashSim s_strict_sim;

static int strict_flash_erase(uint32_t addr) {
    if (addr % STRICT_SECTOR_SIZE != 0U) {
        return -1;
    }
    uint32_t sec = addr / STRICT_SECTOR_SIZE;
    if (sec >= STRICT_SECTOR_COUNT) {
        return -1;
    }
    memset(&s_strict_sim.buffer[sec * STRICT_SECTOR_SIZE], s_strict_sim.erased_byte,
           STRICT_SECTOR_SIZE);
    return 0;
}

static int strict_flash_read(uint32_t addr, void *buf, size_t len) {
    if ((addr + len) > sizeof(s_strict_sim.buffer)) {
        return -1;
    }
    memcpy(buf, &s_strict_sim.buffer[addr], len);
    return 0;
}

static int strict_flash_write(uint32_t addr, const void *buf, size_t len) {
    if ((addr + len) > sizeof(s_strict_sim.buffer)) {
        return -1;
    }
    /* Strictly enforce program_granule alignment and length multiple */
    if ((addr % s_strict_sim.granule != 0U) || (len % s_strict_sim.granule != 0U)) {
        return -1;
    }
    if (s_strict_sim.fail_after_write_count >= 0) {
        if (s_strict_sim.write_count >= s_strict_sim.fail_after_write_count) {
            return -1; /* Power loss injection */
        }
        s_strict_sim.write_count++;
    }
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0U; i < len; ++i) {
        s_strict_sim.buffer[addr + i] &= src[i];
    }
    return 0;
}

static uint32_t strict_flash_sector_size(uint32_t addr) {
    (void)addr;
    return STRICT_SECTOR_SIZE;
}

static void test_issue_91_flash_granules_and_power_loss(void) {
    static const uint8_t granules[] = {2U, 4U, 8U, 16U, 32U};
    for (size_t g_idx = 0U; g_idx < sizeof(granules) / sizeof(granules[0]); ++g_idx) {
        uint8_t g = granules[g_idx];
        memset(&s_strict_sim, 0xFF, sizeof(s_strict_sim));
        s_strict_sim.granule = g;
        s_strict_sim.erased_byte = 0xFFU;
        s_strict_sim.fail_after_write_count = -1;
        s_strict_sim.write_count = 0;

        UdsFlashPort port = {
            .erase = strict_flash_erase,
            .read = strict_flash_read,
            .write = strict_flash_write,
            .sector_size = strict_flash_sector_size,
            .program_granule = g,
            .erased_byte = 0xFFU,
        };

        UdsParamStore store;
        assert(uds_param_init(&store, &port, 0U, STRICT_SECTOR_COUNT, sizeof(TestData)) ==
               UDS_PARAM_OK);
        assert(store.program_granule == g);
        assert(store.slot_size % g == 0U);

        TestData d1 = {.sensor_val = (uint16_t)(0x1000U + g), .flags = 0xAAU, .mode = 0x01U};
        assert(uds_param_save(&store, &d1) == UDS_PARAM_OK);

        TestData d1_read;
        assert(uds_param_load(&store, &d1_read) == UDS_PARAM_OK);
        assert(d1_read.sensor_val == (uint16_t)(0x1000U + g));

        /* Test power loss: inject failure during subsequent save */
        s_strict_sim.write_count = 0;
        s_strict_sim.fail_after_write_count = 0; /* Fail on the very first write of next save */
        TestData d2 = {.sensor_val = 0x9999U, .flags = 0xBBU, .mode = 0x02U};
        assert(uds_param_save(&store, &d2) != UDS_PARAM_OK);

        /* Recover after power loss / reboot */
        s_strict_sim.fail_after_write_count = -1;
        UdsParamStore recovered_store;
        assert(uds_param_init(&recovered_store, &port, 0U, STRICT_SECTOR_COUNT, sizeof(TestData)) ==
               UDS_PARAM_OK);
        assert(recovered_store.has_active_slot);

        TestData rec_data;
        assert(uds_param_load(&recovered_store, &rec_data) == UDS_PARAM_OK);
        assert(rec_data.sensor_val ==
               (uint16_t)(0x1000U + g)); /* Successfully rolled back to last committed state */
    }
}

static void test_erase_all_and_null_guards(void) {
    UdsParamStore store;
    TestData write_data = {.sensor_val = 0x1234U, .flags = 0x55U, .mode = 0x02U};
    TestData read_data;

    /* NULL guards */
    assert(uds_param_init(NULL, &s_test_flash_port, 0U, 3U, 16U) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_init(&store, NULL, 0U, 3U, 16U) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_load(NULL, &read_data) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_load(&store, NULL) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_save(NULL, &write_data) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_save(&store, NULL) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_erase_all(NULL) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_write_count(NULL) == 0U);

    /* Test uds_param_erase_all */
    memset(s_ram_flash, 0xFF, sizeof(s_ram_flash));
    assert(uds_param_init(&store, &s_test_flash_port, 0U, TEST_SECTOR_COUNT, sizeof(TestData)) ==
           UDS_PARAM_OK);
    assert(uds_param_save(&store, &write_data) == UDS_PARAM_OK);
    assert(store.has_active_slot);
    assert(uds_param_write_count(&store) == 1U);

    assert(uds_param_erase_all(&store) == UDS_PARAM_OK);
    assert(!store.has_active_slot);
    assert(uds_param_write_count(&store) == 0U);

    /* Load when not active */
    assert(uds_param_load(&store, &read_data) == UDS_PARAM_ERR);

    /* Test CRC with NULL data */
    assert(uds_crc16_ccitt_update(0x1234U, NULL, 10U) == 0x1234U);

    /* Test invalid port configuration */
    UdsFlashPort invalid_port = s_test_flash_port;
    invalid_port.erase = NULL;
    assert(uds_param_init(&store, &invalid_port, 0U, 2U, 8U) == UDS_PARAM_INVALID_PARAM);
    invalid_port = s_test_flash_port;
    invalid_port.read = NULL;
    assert(uds_param_init(&store, &invalid_port, 0U, 2U, 8U) == UDS_PARAM_INVALID_PARAM);
    invalid_port = s_test_flash_port;
    invalid_port.write = NULL;
    assert(uds_param_init(&store, &invalid_port, 0U, 2U, 8U) == UDS_PARAM_INVALID_PARAM);
    invalid_port = s_test_flash_port;
    invalid_port.sector_size = NULL;
    assert(uds_param_init(&store, &invalid_port, 0U, 2U, 8U) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_init(&store, &s_test_flash_port, 0U, 0U, 8U) == UDS_PARAM_INVALID_PARAM);
    assert(uds_param_init(&store, &s_test_flash_port, 0U, 2U, 0U) == UDS_PARAM_INVALID_PARAM);

    /* Test non-power-of-2 granule */
    invalid_port = s_test_flash_port;
    invalid_port.program_granule = 3U;
    assert(uds_param_init(&store, &invalid_port, 0U, 2U, 8U) == UDS_PARAM_INVALID_PARAM);

    /* Test slots_per_sector < 2 (data size too large for sector) */
    assert(uds_param_init(&store, &s_test_flash_port, 0U, 2U, TEST_SECTOR_SIZE) ==
           UDS_PARAM_INVALID_PARAM);
}

int main(void) {
    test_crc16();
    test_basic_wear_leveling();
    test_sector_rotation();
    test_power_loss_recovery();
    test_sequence_rollover();
    test_dtc_persistence_integration();
    test_issue_91_flash_granules_and_power_loss();
    test_erase_all_and_null_guards();
    return 0;
}
