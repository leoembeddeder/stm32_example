/*
 * SPDX-License-Identifier: LicenseRef-STM32-UDS-Research-Education-Commercial-1.0
 */
#include "uds_iso_tp/uds_services.h"

#include <stddef.h>

UdsCallbackResult uds_service_backends_preflight(const UdsServiceBackends *backends, void *context,
                                                 uint8_t sid, const uint8_t *request,
                                                 uint16_t request_length) {
    if (backends == NULL)
        return UDS_RESULT_NOT_SUPPORTED;
    if ((sid == 0x23U) || (sid == 0x3DU)) {
        if ((backends->memory == NULL) || (backends->memory->check_access == NULL))
            return UDS_RESULT_OK;
        return backends->memory->check_access(context, sid, request, request_length);
    }
    if ((sid == 0x2AU) || (sid == 0x86U)) {
        if ((backends->periodic_event == NULL) ||
            (backends->periodic_event->max_pending_items == 0U))
            return UDS_RESULT_NOT_SUPPORTED;
    }
    return UDS_RESULT_OK;
}

typedef UdsServiceHandlerFn (*UdsBackendExtractorFn)(const UdsServiceBackends *b);

static UdsServiceHandlerFn get_mem_read(const UdsServiceBackends *b) {
    return (b->memory != NULL) ? b->memory->read_memory : NULL;
}
static UdsServiceHandlerFn get_mem_write(const UdsServiceBackends *b) {
    return (b->memory != NULL) ? b->memory->write_memory : NULL;
}
static UdsServiceHandlerFn get_did_scaling(const UdsServiceBackends *b) {
    return (b->did != NULL) ? b->did->read_scaling : NULL;
}
static UdsServiceHandlerFn get_did_dynamic(const UdsServiceBackends *b) {
    return (b->did != NULL) ? b->did->dynamically_define : NULL;
}
static UdsServiceHandlerFn get_xfer_upload(const UdsServiceBackends *b) {
    return (b->transfer != NULL) ? b->transfer->request_upload : NULL;
}
static UdsServiceHandlerFn get_xfer_file(const UdsServiceBackends *b) {
    return (b->transfer != NULL) ? b->transfer->request_file : NULL;
}
static UdsServiceHandlerFn get_timing(const UdsServiceBackends *b) {
    return (b->timing != NULL) ? b->timing->access_timing_parameters : NULL;
}
static UdsServiceHandlerFn get_periodic_data(const UdsServiceBackends *b) {
    return (b->periodic_event != NULL) ? b->periodic_event->periodic_data : NULL;
}
static UdsServiceHandlerFn get_periodic_event(const UdsServiceBackends *b) {
    return (b->periodic_event != NULL) ? b->periodic_event->event_response : NULL;
}
static UdsServiceHandlerFn get_link_ctrl(const UdsServiceBackends *b) {
    return (b->link_control != NULL) ? b->link_control->link_control : NULL;
}
static UdsServiceHandlerFn get_auth(const UdsServiceBackends *b) {
    return (b->authentication != NULL) ? b->authentication->authentication : NULL;
}
static UdsServiceHandlerFn get_sec_data(const UdsServiceBackends *b) {
    return (b->secured_data != NULL) ? b->secured_data->secured_data_transmission : NULL;
}

typedef struct {
    uint8_t sid;
    UdsBackendExtractorFn extract;
} UdsBackendSidMapping;

static const UdsBackendSidMapping k_sid_mappings[] = {
    { 0x23U, get_mem_read },
    { 0x3DU, get_mem_write },
    { 0x24U, get_did_scaling },
    { 0x2CU, get_did_dynamic },
    { 0x35U, get_xfer_upload },
    { 0x38U, get_xfer_file },
    { 0x83U, get_timing },
    { 0x2AU, get_periodic_data },
    { 0x86U, get_periodic_event },
    { 0x87U, get_link_ctrl },
    { 0x29U, get_auth },
    { 0x84U, get_sec_data },
};

UdsServiceHandlerFn uds_service_backends_handler(const UdsServiceBackends *backends, uint8_t sid) {
    if (backends == NULL) {
        return NULL;
    }
    for (size_t i = 0U; i < (sizeof(k_sid_mappings) / sizeof(k_sid_mappings[0])); ++i) {
        if (k_sid_mappings[i].sid == sid) {
            return k_sid_mappings[i].extract(backends);
        }
    }
    return NULL;
}
