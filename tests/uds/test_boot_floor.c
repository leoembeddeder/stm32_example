#include "boot_test_util.h"
#include "uds_bootloader.h"
#include "uds_iso_tp/boot_floor.h"

#include <assert.h>
#include <string.h>

/* ---- strict fake flash (2 x 1 KiB), same rules as real NOR flash ---- */
#define FLASH_BASE 0x08020000UL
#define SECTOR_SZ 1024U
static uint8_t s_flash[2U * SECTOR_SZ];
static bool s_fail_writes;

static int f_erase(uint32_t a) {
    uint32_t off = (a - (uint32_t)FLASH_BASE) / SECTOR_SZ * SECTOR_SZ;
    (void)memset(&s_flash[off], 0xFF, SECTOR_SZ);
    return 0;
}
static int f_read(uint32_t a, void *b, size_t n) {
    (void)memcpy(b, &s_flash[a - (uint32_t)FLASH_BASE], n);
    return 0;
}
static int f_write(uint32_t a, const void *b, size_t n) {
    if (s_fail_writes) {
        return -1;
    }
    const uint32_t off = a - (uint32_t)FLASH_BASE;
    assert((off % 8U) == 0U);
    assert((n % 8U) == 0U);
    for (size_t i = 0U; i < n; ++i) {
        assert(s_flash[off + i] == 0xFFU);
    }
    (void)memcpy(&s_flash[off], b, n);
    return 0;
}
static uint32_t f_sector(uint32_t a) {
    (void)a;
    return SECTOR_SZ;
}
static const UdsFlashPort s_port = {f_erase, f_read, f_write, f_sector, 8U, 0xFFU};

static void reset_flash(void) {
    (void)memset(s_flash, 0xFF, sizeof(s_flash));
    s_fail_writes = false;
}

static void test_floor_module(void) {
    reset_flash();
    UdsBootFloor bf;
    assert(boot_floor_init(&bf, &s_port, FLASH_BASE, 2U));
    assert(boot_floor_get(&bf) == 0U); /* first boot */
    assert(boot_floor_raise(&bf, 5U));
    assert(boot_floor_raise(&bf, 3U)); /* lowering is ignored, not an error */
    assert(boot_floor_get(&bf) == 5U);

    UdsBootFloor again; /* power cycle */
    assert(boot_floor_init(&again, &s_port, FLASH_BASE, 2U));
    assert(boot_floor_get(&again) == 5U);

    s_fail_writes = true; /* failed write must not move the floor */
    assert(!boot_floor_raise(&again, 9U));
    assert(boot_floor_get(&again) == 5U);
    s_fail_writes = false;

    /* Unusable storage fails closed: nothing may be installed. */
    UdsBootFloor bad;
    assert(!boot_floor_init(&bad, &s_port, FLASH_BASE, 1U));
    assert(boot_floor_get(&bad) == UINT32_MAX);
    assert(boot_floor_get(NULL) == UINT32_MAX);
    assert(!boot_floor_raise(&bad, 1U));
}

static UdsCallbackResult check_memory(const FirmwareMetadata_t *m, uint8_t *status) {
    uint8_t out[16];
    uint16_t out_len = 0U;
    UdsCallbackResult r =
        uds_bootloader_routine_control(NULL, 0x01U, UDS_BL_ROUTINE_CHECK_MEMORY, (const uint8_t *)m,
                                       sizeof(*m), out, &out_len, sizeof(out));
    *status = out[0];
    return r;
}

static void test_bootloader_enforces_persistent_floor(void) {
    reset_flash();
    UdsBootFloor bf;
    assert(boot_floor_init(&bf, &s_port, FLASH_BASE, 2U));
    uds_bootloader_init();
    uds_bootloader_set_target(UDS_BL_TARGET_STM32C092);
    uds_bootloader_set_floor_store(&bf);

    /* Install and confirm v5. */
    FirmwareMetadata_t meta;
    uint8_t st = 0U;
    bt_stage_image(&meta, 5U);
    bt_sign(&meta);
    assert(check_memory(&meta, &st) == UDS_RESULT_OK);
    assert(uds_bootloader_activate_candidate() == UDS_DOWNLOAD_OK);
    assert(boot_floor_get(&bf) < 5U); /* not raised until the image is CONFIRMED */
    assert(uds_bootloader_confirm_active_image() == UDS_DOWNLOAD_OK);
    assert(boot_floor_get(&bf) == 5U);

    /* Crash-recovery rollback wipes the RAM version (the old code's only floor) ... */
    assert(uds_bootloader_rollback_candidate() == UDS_DOWNLOAD_OK);
    assert(uds_bootloader_get_active_version() == 1U);
    assert(uds_bootloader_get_version_floor() == 5U); /* ... but the persistent floor holds. */

    /* Power cycle: brand-new floor object over the same flash. */
    UdsBootFloor after;
    assert(boot_floor_init(&after, &s_port, FLASH_BASE, 2U));
    uds_bootloader_set_floor_store(&after);

    /* An older signed image (v3) is refused, and so is a relabelled-to-v5-or-higher forgery. */
    bt_stage_image(&meta, 3U);
    bt_sign(&meta);
    assert(check_memory(&meta, &st) == UDS_RESULT_OUT_OF_RANGE);
    assert(st == 0x02U);
    assert(!uds_bootloader_is_activation_pending());

    FirmwareMetadata_t forged = meta;
    forged.version = 7U; /* signature no longer matches the manifest */
    boot_finalize_metadata(&forged);
    assert(check_memory(&forged, &st) == UDS_RESULT_SECURITY_DENIED);
    assert(st == 0x04U);

    /* A genuinely newer signed image is accepted. */
    bt_stage_image(&meta, 6U);
    bt_sign(&meta);
    assert(check_memory(&meta, &st) == UDS_RESULT_OK);

    /* If the floor cannot be persisted, the image is NOT confirmed (fail closed). */
    assert(uds_bootloader_activate_candidate() == UDS_DOWNLOAD_OK);
    s_fail_writes = true;
    assert(uds_bootloader_confirm_active_image() == UDS_DOWNLOAD_PROGRAM_ERROR);
    s_fail_writes = false;
    assert(uds_bootloader_confirm_active_image() == UDS_DOWNLOAD_OK);
    assert(boot_floor_get(&after) == 6U);

    uds_bootloader_set_floor_store(NULL);
}

int main(void) {
    test_floor_module();
    test_bootloader_enforces_persistent_floor();
    return 0;
}
