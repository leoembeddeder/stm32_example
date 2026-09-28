---
name: Porting & Board Support Request
about: Ask questions or request assistance porting to a new MCU target
title: '[PORT] '
labels: 'support'
assignees: ''
---

**Target Microcontroller**
- Target Family & Part Number (e.g. STM32F103, STM32G431, NXP S32K144):
- CAN Peripheral Type (e.g. bxCAN, FDCAN, FlexCAN):
- Flash Programming Granularity (2, 4, 8, or 16 bytes):

**Have you checked the Porting Guide?**
Please review [docs/porting_guide.md](../docs/porting_guide.md) before opening this request. Implementing `can_port.h`, `flash_port.h`, `clock_port.h`, and `reset_port.h` is all that is required to integrate any microcontroller with the core library.

**Specific Question or Blockers**
Describe the specific area where you need assistance (e.g., linker script, vector table relocation, FDCAN message RAM configuration).
