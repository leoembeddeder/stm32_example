/**
 * @file can_frame_fd.h
 * @brief CAN and CAN-FD Frame Data Structure supporting up to 64-byte payloads (ISO 11898-1:2015)
 */

#ifndef CAN_FRAME_FD_H
#define CAN_FRAME_FD_H

#include <stdint.h>
#include <stdbool.h>

#define CAN_FD_MAX_DLEN 64U

typedef struct {
    uint32_t id;                       /**< 11-bit standard or 29-bit extended CAN identifier */
    uint8_t  dlc;                      /**< Payload length in bytes (0..64) */
    uint8_t  data[CAN_FD_MAX_DLEN];    /**< Payload data bytes */
    bool     is_ext;                   /**< True if 29-bit extended ID */
    bool     is_rtr;                   /**< True if Remote Transmission Request */
    bool     is_fd;                    /**< True if CAN-FD frame (allows DLC > 8) */
    bool     brs;                      /**< Bit Rate Switch flag */
} can_frame_t;

/**
 * @brief Convert byte length (0..64) to valid CAN-FD DLC byte length
 */
static inline uint8_t can_fd_pad_len(uint8_t len) {
    if (len <= 8U)  return len;
    if (len <= 12U) return 12U;
    if (len <= 16U) return 16U;
    if (len <= 20U) return 20U;
    if (len <= 24U) return 24U;
    if (len <= 32U) return 32U;
    if (len <= 48U) return 48U;
    return 64U;
}

#endif /* CAN_FRAME_FD_H */
