#ifndef SVC_ROUTINE_CONTROL_H
#define SVC_ROUTINE_CONTROL_H

#include "svc_read_dtc.h"

#define ROUTINE_ERASE_MEMORY   0xFF00U
#define ROUTINE_CHECK_MEMORY   0xFF01U
#define ROUTINE_ACTIVATE       0x0203U
#define ROUTINE_ROLLBACK       0x0204U

#define NRC_REQUEST_SEQUENCE_ERROR 0x24U

void svc_routine_control(const uds_request_t *req, uds_response_t *resp);

#endif /* SVC_ROUTINE_CONTROL_H */
