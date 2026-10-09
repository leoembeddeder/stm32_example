/**
 * @file svc_clear_dtc.h
 * @brief UDS Service 0x14: ClearDiagnosticInformation (ISO 14229-1)
 */

#ifndef SVC_CLEAR_DTC_H
#define SVC_CLEAR_DTC_H

#include "uds_types.h"
#include <stdbool.h>

/**
 * @brief Configure whether session check is enforced (default: true)
 */
void svc_clear_dtc_set_session_check(bool enforce);

/**
 * @brief Service 0x14 handler
 */
void svc_clear_dtc(const uds_request_t *req, uds_response_t *resp);

#endif /* SVC_CLEAR_DTC_H */
