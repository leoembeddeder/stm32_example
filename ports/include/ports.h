#ifndef PORTS_H
#define PORTS_H

#include "can_port.h"
#include "clock_port.h"
#include "flash_port.h"
#include "reset_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* STM32C0 Port */
const ClockPortInterface *ports_stm32c0_clock(void);
const FlashPortInterface *ports_stm32c0_flash(void);

/* STM32F1 Port */
const ClockPortInterface *ports_stm32f1_clock(void);
const FlashPortInterface *ports_stm32f1_flash(void);

/* STM32F4 Port */
const ClockPortInterface *ports_stm32f4_clock(void);
const FlashPortInterface *ports_stm32f4_flash(void);

/* STM32F7 Port */
const ClockPortInterface *ports_stm32f7_clock(void);
const FlashPortInterface *ports_stm32f7_flash(void);

/* STM32G4 Port */
const ClockPortInterface *ports_stm32g4_clock(void);
const FlashPortInterface *ports_stm32g4_flash(void);

#ifdef __cplusplus
}
#endif

#endif /* PORTS_H */
