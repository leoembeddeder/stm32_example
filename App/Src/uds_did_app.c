#include "uds_did_app.h"

#include <string.h>

#define UDS_DID_MAX_DATA_SIZE 64U

typedef struct {
    uint16_t did;
    bool is_writable;
    uint16_t length;
    uint8_t data[UDS_DID_MAX_DATA_SIZE];
} UdsDidRecord;

#define UDS_DID_COUNT 8U

static UdsDidRecord s_did_table[UDS_DID_COUNT];
static uint8_t s_active_session = 0x01U; /* UDS_SESSION_DEFAULT */

void uds_did_app_init(void) {
    (void)memset(s_did_table, 0, sizeof(s_did_table));
    s_active_session = 0x01U;

    /* 0xF186: ActiveDiagnosticSession (1 byte) */
    s_did_table[0].did = UDS_DID_ACTIVE_DIAGNOSTIC_SESSION;
    s_did_table[0].is_writable = false;
    s_did_table[0].length = 1U;
    s_did_table[0].data[0] = s_active_session;

    /* 0xF181: HIBManufacturerSparePartNumber */
    const char *spare_part = "HIB-HCU-SPARE-01";
    size_t spare_len = strlen(spare_part);
    s_did_table[1].did = UDS_DID_HIB_SPARE_PART_NUMBER;
    s_did_table[1].is_writable = false;
    s_did_table[1].length = (uint16_t)spare_len;
    (void)memcpy(s_did_table[1].data, spare_part, spare_len);

    /* 0xF182: Boot SW Identifier */
    const char *boot_sw = "BL-STM32F767-V1.0";
    size_t boot_len = strlen(boot_sw);
    s_did_table[2].did = UDS_DID_BOOT_SW_IDENTIFIER;
    s_did_table[2].is_writable = false;
    s_did_table[2].length = (uint16_t)boot_len;
    (void)memcpy(s_did_table[2].data, boot_sw, boot_len);

    /* 0xF183: HIB Manufacturer ECU Software Number */
    const char *ecu_sw = "HIB-SW-1002";
    size_t ecu_sw_len = strlen(ecu_sw);
    s_did_table[3].did = UDS_DID_ECU_SOFTWARE_NUMBER;
    s_did_table[3].is_writable = true;
    s_did_table[3].length = (uint16_t)ecu_sw_len;
    (void)memcpy(s_did_table[3].data, ecu_sw, ecu_sw_len);

    /* 0xF184: HIB Manufacturer ECU APP Software Number */
    const char *app_sw = "HIB-APP-2004";
    size_t app_sw_len = strlen(app_sw);
    s_did_table[4].did = UDS_DID_ECU_APP_SOFTWARE_NUMBER;
    s_did_table[4].is_writable = true;
    s_did_table[4].length = (uint16_t)app_sw_len;
    (void)memcpy(s_did_table[4].data, app_sw, app_sw_len);

    /* 0xF185: HIB Manufacturer ECU Hardware Number */
    const char *hw_num = "HIB-HW-03";
    size_t hw_len = strlen(hw_num);
    s_did_table[5].did = UDS_DID_ECU_HARDWARE_NUMBER;
    s_did_table[5].is_writable = false;
    s_did_table[5].length = (uint16_t)hw_len;
    (void)memcpy(s_did_table[5].data, hw_num, hw_len);

    /* 0xF18A: systemSupplierIdentifierDataIdentifier */
    const char *supplier = "SUPPLIER-ELCTR";
    size_t sup_len = strlen(supplier);
    s_did_table[6].did = UDS_DID_SYSTEM_SUPPLIER_IDENTIFIER;
    s_did_table[6].is_writable = false;
    s_did_table[6].length = (uint16_t)sup_len;
    (void)memcpy(s_did_table[6].data, supplier, sup_len);

    /* 0xF190: VIN (17 bytes) */
    const char *vin = "stm32f767-uds-vin";
    size_t vin_len = strlen(vin);
    s_did_table[7].did = UDS_DID_VIN;
    s_did_table[7].is_writable = true;
    s_did_table[7].length = (uint16_t)vin_len;
    (void)memcpy(s_did_table[7].data, vin, vin_len);
}

void uds_did_app_set_active_session(uint8_t session) {
    s_active_session = session;
    s_did_table[0].data[0] = session;
}

UdsCallbackResult uds_did_app_read(void *context, uint16_t did, uint8_t *data, uint16_t *length,
                                   uint16_t capacity) {
    (void)context;
    if ((data == NULL) || (length == NULL)) {
        return UDS_RESULT_ERROR;
    }

    if (did == UDS_DID_ACTIVE_DIAGNOSTIC_SESSION) {
        s_did_table[0].data[0] = s_active_session;
    }

    for (size_t i = 0U; i < UDS_DID_COUNT; ++i) {
        if (s_did_table[i].did == did) {
            if (capacity < s_did_table[i].length) {
                return UDS_RESULT_RESPONSE_TOO_LONG;
            }
            (void)memcpy(data, s_did_table[i].data, s_did_table[i].length);
            *length = s_did_table[i].length;
            return UDS_RESULT_OK;
        }
    }
    return UDS_RESULT_OUT_OF_RANGE;
}

UdsCallbackResult uds_did_app_write(void *context, uint16_t did, const uint8_t *data,
                                    uint16_t length) {
    (void)context;
    if ((data == NULL) || (length == 0U) || (length > UDS_DID_MAX_DATA_SIZE)) {
        return UDS_RESULT_OUT_OF_RANGE;
    }

    for (size_t i = 0U; i < UDS_DID_COUNT; ++i) {
        if (s_did_table[i].did == did) {
            if (!s_did_table[i].is_writable) {
                return UDS_RESULT_DENIED;
            }
            (void)memcpy(s_did_table[i].data, data, length);
            s_did_table[i].length = length;
            return UDS_RESULT_OK;
        }
    }
    return UDS_RESULT_OUT_OF_RANGE;
}