/**
 * @file svc_read_dtc.h
 * @brief UDS Service 0x19: ReadDTCInformation
 */

#ifndef SVC_READ_DTC_H
#define SVC_READ_DTC_H

#include "uds_types.h"

void svc_read_dtc_info(const uds_request_t *req, uds_response_t *resp);

#endif /* SVC_READ_DTC_H */
