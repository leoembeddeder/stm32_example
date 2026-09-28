#ifndef CLOCK_PORT_H
#define CLOCK_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard MCU hardware timing and microsecond tick interface.
 */
typedef struct {
    uint32_t (*get_tick_ms)(void);
    uint64_t (*get_tick_us)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
} ClockPortInterface;

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_PORT_H */
