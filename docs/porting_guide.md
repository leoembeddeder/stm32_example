# MCU Porting Guide for STM32 UDS ISO-TP

This guide describes how to port the freestanding, HAL-free core `library/` to any new microcontroller family (such as STM32F1, STM32F4, STM32G4, NXP S32K, or other ARM Cortex-M devices).

---

## Architecture Overview
The repository strictly separates pure protocol logic from MCU hardware peripherals:
```
library/   HAL-free, host-tested: isotp, uds core, dtc, did, security,
           bootloader state machine, flash fsm, wear leveling, crypto
ports/     one folder per MCU family: f1, f4, f7, c0, g4
           each implements: can_port.h, flash_port.h, clock_port.h, reset_port.h
examples/  thin main() + board configuration only
```

---

## 4 Standard Port Interfaces to Implement

To add support for a new MCU, create a directory under `ports/<target_family>/` and implement the following 4 interfaces:

### 1. `can_port.h` - CAN Hardware Driver
Normalized frame transmission and reception:
```c
typedef struct {
    bool (*init)(uint32_t baud_rate);
    bool (*send)(const CanPortMessage *msg);
    bool (*receive)(CanPortMessage *msg);
    void (*deinit)(void);
} CanPortInterface;
```
- Implement standard 11-bit identifier filtering for physical (`0x7E0`) and functional (`0x7DF`) IDs.
- For CAN-FD targets (e.g. STM32G4/C092), configure message RAM and bit rate switching (BRS).

### 2. `flash_port.h` - Non-Volatile Memory Driver
Exposes on-chip flash write granularity (Issue #91) and page/sector erase:
```c
typedef struct {
    uint8_t program_granule; /* 2 = Halfword, 4 = Word, 8 = Doubleword */
    uint8_t erased_byte;     /* 0xFF */
    bool (*unlock)(void);
    bool (*lock)(void);
    bool (*erase_sector)(uint32_t sector_addr);
    bool (*program)(uint32_t addr, const uint8_t *data, size_t len);
    const uint8_t *(*read)(uint32_t addr, size_t len);
} FlashPortInterface;
```
- For STM32F1: `program_granule = 2U` (16-bit halfword).
- For STM32F4/F7: `program_granule = 4U` (32-bit word).
- For STM32C0/G4: `program_granule = 8U` (64-bit doubleword).

### 3. `clock_port.h` - System Timing
Provides millisecond and microsecond monotonic timers:
```c
typedef struct {
    uint32_t (*get_tick_ms)(void);
    uint64_t (*get_tick_us)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
} ClockPortInterface;
```
- Typically backed by SysTick or a 32-bit hardware general-purpose timer (TIM2/TIM5).

### 4. `reset_port.h` - System Reset & App Jump
Handles software reboot and vector table relocation:
```c
typedef struct {
    void (*system_reset)(void);
    void (*jump_to_app)(uint32_t app_vector_addr);
    bool (*is_app_valid)(uint32_t app_vector_addr);
} ResetPortInterface;
```
- Cleans and disables interrupts (NVIC ICER/ICPR).
- On Cortex-M7: flushes and invalidates L1 I/D-cache (`SCB_DisableDCache()`).
- Relocates `SCB->VTOR` to the target application slot.
- Resets MSP (`__set_MSP`) and branches to application reset handler.
