#ifndef SVC_WRITE_ADDRESS_H
#define SVC_WRITE_ADDRESS_H

#include "svc_read_dtc.h"

#define NRC_SECURITY_ACCESS_DENIED 0x33U

void svc_write_memory_by_address(const uds_request_t *req, uds_response_t *resp);

#endif /* SVC_WRITE_ADDRESS_H */
