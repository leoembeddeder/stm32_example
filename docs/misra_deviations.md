# MISRA C:2012 Compliance and Deviation Record

## 1. Scope & Objective
This document outlines the MISRA C:2012 compliance strategy, active ruleset, and formal deviations for the `stm32_uds_iso_tp` freestanding protocol stack. The stack adheres strictly to automotive safety coding principles:
- Fixed-width standard integer types (`stdint.h`: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`).
- Explicit integer constant qualification (`U` and `UL` suffixes).
- Deterministic execution: **Zero dynamic heap memory allocation** (`malloc`, `free` forbidden).
- Static bounded buffers for all ISO-TP reassembly and UDS diagnostic response buffers.
- Cyclomatic Complexity Number (CCN) bounded: `CCN <= 15` across all functions.

---

## 2. Core MISRA C:2012 Enforced Rules

| Rule | Classification | Description | Status |
|------|----------------|-------------|--------|
| **Rule 2.1** | Required | A project shall not contain unreachable code | **Compliant** |
| **Rule 2.2** | Required | There shall be no dead code | **Compliant** |
| **Rule 4.6** | Advisory | Typedefs that indicate size and signedness should be used | **Compliant** (`<stdint.h>`) |
| **Rule 8.4** | Required | A compatible declaration shall be visible when an object/function with external linkage is defined | **Compliant** |
| **Rule 9.1** | Mandatory | The value of an object with automatic storage duration shall not be read before being set | **Compliant** |
| **Rule 10.1** | Required | Operands shall not be of an inappropriate essential type category | **Compliant** |
| **Rule 10.3** | Required | The value of an expression shall not be assigned to an object with a narrower essential type | **Compliant** (Explicit casts) |
| **Rule 10.4** | Required | Both operands of an operator in which usual arithmetic conversions are performed shall have the same essential type | **Compliant** |
| **Rule 14.4** | Required | The condition of an if-statement and the condition of an iteration-statement shall have type bool | **Compliant** |
| **Rule 15.6** | Required | The body of an iteration-statement or a selection-statement shall be a compound statement | **Compliant** (Braces required) |
| **Rule 17.7** | Required | The value returned by a function having non-void return type shall be used | **Compliant** (Explicit `(void)` cast when ignored) |
| **Rule 21.3** | Required | The memory allocation and deallocation functions of `<stdlib.h>` shall not be used | **Compliant** (No dynamic allocation) |

---

## 3. Formal Deviations List

### Deviation 1: Pointer to Integer / Integer to Pointer Conversion
- **MISRA Rule**: Rule 11.4 (Advisory) / Rule 11.6 (Required)
  - *"A conversion should not be performed between a pointer to object and an integer type."*
- **Locations**:
  - `ports/stm32f7/port_stm32f7.c`, `ports/stm32c0/port_stm32c0.c`, `App/Src/boot_jump.c`, `App/Src/uds_bootloader.c`
- **Rationale**:
  - Automotive microcontrollers require access to physical flash and SRAM memory-mapped addresses (e.g. `0x08000000UL`, `0x20000000UL`) and vector table VTOR offsets.
- **Mitigation**:
  - Conversions are restricted to validated address boundaries (verified against flash and RAM boundaries) using `uintptr_t` intermediate casts.

### Deviation 2: Void Pointer Casts for Generic Context
- **MISRA Rule**: Rule 11.5 (Advisory)
  - *"A conversion should not be performed from pointer to void into pointer to object."*
- **Locations**:
  - `library/src/uds.c`, `library/src/endpoint.c`
- **Rationale**:
  - Standard UDS and ISO-TP callback interfaces pass `void *context` to support arbitrary user/application states without tying the core protocol stack to application data types.
- **Mitigation**:
  - Type-safe wrapper macros and callback functions validate non-null state before casting back to expected subsystem context.

### Deviation 3: Bitwise Operations on Signed Types
- **MISRA Rule**: Rule 10.1 (Required)
  - *"Bitwise operations shall not be performed on signed types."*
- **Locations**:
  - Implicit promotion of `uint8_t` / `uint16_t` in bit-shift expressions (`<<`, `>>`, `|`).
- **Rationale**:
  - Standard C promotes sub-int integer types to signed `int`.
- **Mitigation**:
  - Explicit casts to `(uint32_t)` are applied to all sub-expressions before bitwise operations, complying with strict `-Wconversion` and `-Wsign-conversion`.
