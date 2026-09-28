#ifndef CAN_PORT_H
#define CAN_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Normalized CAN/CAN-FD frame representation across MCU targets.
 */
typedef struct {
    uint32_t id;
    bool is_extended;
    bool is_fd;
    uint8_t dlc;
    uint8_t data[64];
} CanPortMessage;

/**
 * @brief Standard MCU hardware CAN peripheral port interface.
 */
typedef struct {
    bool (*init)(uint32_t baud_rate);
    bool (*send)(const CanPortMessage *msg);
    bool (*receive)(CanPortMessage *msg);
    void (*deinit)(void);
} CanPortInterface;

#ifdef __cplusplus
}
#endif

#endif /* CAN_PORT_H */
