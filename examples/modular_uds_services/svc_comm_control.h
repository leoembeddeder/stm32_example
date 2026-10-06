#ifndef SVC_COMM_CONTROL_H
#define SVC_COMM_CONTROL_H

#include "svc_read_dtc.h"

void svc_comm_control(const uds_request_t *req, uds_response_t *resp);
void svc_comm_control_reset(void);
uint8_t svc_comm_control_get_subfunction(void);
uint8_t svc_comm_control_get_type(void);

#endif /* SVC_COMM_CONTROL_H */
