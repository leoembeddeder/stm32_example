#ifndef SVC_CONTROL_DTC_H
#define SVC_CONTROL_DTC_H

#include "svc_read_dtc.h"

void svc_control_dtc(const uds_request_t *req, uds_response_t *resp);
bool dtc_is_setting_enabled(void);

#endif /* SVC_CONTROL_DTC_H */
