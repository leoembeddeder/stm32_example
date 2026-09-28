/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_iso_tp/uds_dtc.h"

#include <stddef.h>

typedef struct {
    uint8_t subfunction;
    uint8_t min_len;
    uint8_t max_len;
    uint32_t capability;
} UdsDtcSubfnMeta;

static const UdsDtcSubfnMeta k_dtc_subfn_table[] = {
    { 0x01U, 3U, 3U, UDS_DTC_CAP_REPORT_NUMBER_BY_STATUS },
    { 0x02U, 3U, 3U, UDS_DTC_CAP_REPORT_BY_STATUS_MASK },
    { 0x03U, 2U, 2U, UDS_DTC_CAP_REPORT_SNAPSHOT_IDENTIFICATION },
    { 0x04U, 5U, 6U, UDS_DTC_CAP_REPORT_SNAPSHOT_RECORDS },
    { 0x05U, 3U, 3U, UDS_DTC_CAP_REPORT_MIRROR_MEMORY },
    { 0x06U, 5U, 6U, UDS_DTC_CAP_REPORT_EXTENDED_DATA },
    { 0x07U, 3U, 4U, UDS_DTC_CAP_REPORT_NUMBER_BY_SEVERITY },
    { 0x08U, 3U, 4U, UDS_DTC_CAP_REPORT_BY_SEVERITY },
    { 0x09U, 5U, 5U, UDS_DTC_CAP_REPORT_SEVERITY_INFORMATION },
    { 0x0AU, 2U, 3U, UDS_DTC_CAP_REPORT_SUPPORTED_DTC },
    { 0x0BU, 2U, 2U, UDS_DTC_CAP_REPORT_FIRST_FAILED },
    { 0x0CU, 2U, 2U, UDS_DTC_CAP_REPORT_FIRST_CONFIRMED },
    { 0x0DU, 2U, 2U, UDS_DTC_CAP_REPORT_MOST_RECENT_FAILED },
    { 0x0EU, 2U, 2U, UDS_DTC_CAP_REPORT_MOST_RECENT_CONFIRMED },
    { 0x0FU, 3U, 3U, UDS_DTC_CAP_REPORT_MIRROR_EXTENDED_DATA },
    { 0x10U, 5U, 6U, UDS_DTC_CAP_REPORT_MIRROR_EXTENDED_DATA },
    { 0x11U, 3U, 3U, UDS_DTC_CAP_REPORT_NUMBER_BY_SEVERITY_MASK },
    { 0x12U, 3U, 3U, UDS_DTC_CAP_REPORT_BY_SEVERITY_MASK },
    { 0x13U, 3U, 3U, UDS_DTC_CAP_REPORT_USER_MEMORY },
    { 0x14U, 2U, 2U, UDS_DTC_CAP_REPORT_USER_MEMORY_EXTENDED_DATA },
    { 0x15U, 2U, 3U, UDS_DTC_CAP_REPORT_PERMANENT_STATUS },
    { 0x16U, 3U, 3U, UDS_DTC_CAP_REPORT_PERMANENT_STATUS_MASK },
    { 0x17U, 3U, 4U, UDS_DTC_CAP_REPORT_WWHOBD_STATUS },
    { 0x18U, 6U, 7U, UDS_DTC_CAP_REPORT_WWHOBD_STATUS_MASK },
    { 0x19U, 6U, 7U, UDS_DTC_CAP_REPORT_BY_SEVERITY_RECORDS },
    { 0x42U, 5U, 5U, UDS_DTC_CAP_CUSTOM_42 },
    { 0x55U, 2U, 3U, UDS_DTC_CAP_CUSTOM_55 },
};

static const UdsDtcSubfnMeta *uds_dtc_find_meta(uint8_t subfunction) {
    for (size_t i = 0U; i < (sizeof(k_dtc_subfn_table) / sizeof(k_dtc_subfn_table[0])); ++i) {
        if (k_dtc_subfn_table[i].subfunction == subfunction) {
            return &k_dtc_subfn_table[i];
        }
    }
    return NULL;
}

bool uds_dtc_subfunction_supported(uint8_t subfunction) {
    return uds_dtc_find_meta(subfunction) != NULL;
}

bool uds_dtc_request_length_valid(uint8_t subfunction, uint16_t request_length) {
    const UdsDtcSubfnMeta *meta = uds_dtc_find_meta(subfunction);
    if (meta == NULL) {
        return false;
    }
    return (request_length >= (uint16_t)meta->min_len) && (request_length <= (uint16_t)meta->max_len);
}

uint32_t uds_dtc_capability_for_subfunction(uint8_t subfunction) {
    const UdsDtcSubfnMeta *meta = uds_dtc_find_meta(subfunction);
    return (meta != NULL) ? meta->capability : 0U;
}

bool uds_dtc_backend_supports(const UdsDtcBackend *backend, uint8_t subfunction) {
    uint32_t required = uds_dtc_capability_for_subfunction(subfunction);
    return (backend != NULL) && (backend->report != NULL) && (required != 0U) &&
           ((backend->capabilities & required) != 0U);
}
