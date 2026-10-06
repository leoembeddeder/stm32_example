#ifndef SVC_READ_DTC_H
#define SVC_READ_DTC_H

#include <stdint.h>
#include <stdbool.h>

#ifndef ISOTP_TX_BUF_SIZE
#define ISOTP_TX_BUF_SIZE 4095U
#endif

#define UDS_SID_NEGATIVE_RESPONSE       0x7FU
#define UDS_POSITIVE_RESPONSE(sid)      ((sid) + 0x40U)

#define NRC_SUBFUNCTION_NOT_SUPPORTED   0x12U
#define NRC_INCORRECT_MSG_LEN_OR_FORMAT 0x13U
#define NRC_RESPONSE_TOO_LONG           0x14U
#define NRC_REQUEST_OUT_OF_RANGE        0x31U

typedef struct {
    uint8_t sid;
    const uint8_t *data;
    uint16_t data_len;
    bool functional;
} uds_request_t;

typedef struct {
    uint8_t data[ISOTP_TX_BUF_SIZE];
    uint16_t len;
    bool suppress;
} uds_response_t;

void svc_read_dtc_info(const uds_request_t *req, uds_response_t *resp);

#endif /* SVC_READ_DTC_H */
