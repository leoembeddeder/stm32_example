#include "uds_iso_tp/boot_floor.h"

#include <string.h>

#define FLOOR_MAGIC 0x464C5231UL /* "FLR1" */

typedef struct {
    uint32_t magic;
    uint32_t floor;
    uint32_t check; /* ~(magic ^ floor) */
    uint32_t reserved;
} FloorRecord;

static uint32_t floor_check(const FloorRecord *r) {
    return ~(r->magic ^ r->floor);
}

bool boot_floor_init(UdsBootFloor *bf, const UdsFlashPort *port, uint32_t flash_base,
                     uint8_t sector_count) {
    if (bf == NULL) {
        return false;
    }
    (void)memset(bf, 0, sizeof(*bf));
    bf->floor = UINT32_MAX; /* closed until proven otherwise */
    if ((port == NULL) || (sector_count < 2U)) {
        return false;
    }
    if (uds_param_init(&bf->store, port, flash_base, sector_count, (uint16_t)sizeof(FloorRecord)) !=
        UDS_PARAM_OK) {
        return false;
    }
    bf->ready = true;
    if (!bf->store.has_active_slot) {
        bf->floor = 0U; /* first boot */
        return true;
    }
    FloorRecord rec;
    if ((uds_param_load(&bf->store, &rec) != UDS_PARAM_OK) || (rec.magic != FLOOR_MAGIC) ||
        (rec.check != floor_check(&rec))) {
        return true; /* present but unreadable: stay closed (floor stays UINT32_MAX) */
    }
    bf->floor = rec.floor;
    return true;
}

uint32_t boot_floor_get(const UdsBootFloor *bf) {
    return (bf != NULL) ? bf->floor : UINT32_MAX;
}

bool boot_floor_raise(UdsBootFloor *bf, uint32_t new_floor) {
    if ((bf == NULL) || !bf->ready) {
        return false;
    }
    if (new_floor <= bf->floor) {
        return true; /* monotonic: nothing to do */
    }
    FloorRecord rec;
    (void)memset(&rec, 0, sizeof(rec));
    rec.magic = FLOOR_MAGIC;
    rec.floor = new_floor;
    rec.check = floor_check(&rec);
    if (uds_param_save(&bf->store, &rec) != UDS_PARAM_OK) {
        return false;
    }
    FloorRecord back;
    if ((uds_param_load(&bf->store, &back) != UDS_PARAM_OK) || (back.floor != new_floor)) {
        return false;
    }
    bf->floor = new_floor;
    return true;
}
