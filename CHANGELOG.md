# Changelog

All notable changes to this project are documented here. Host validation and target compatibility remain dependent on the exact compiler, MCU, HAL revision, transceiver, and board configuration.

## [1.6.1] - 2026-09-28

### Added
- **Dedicated Firmware Download Test Suite (`test_download.c`)**:
  - Validates all states (`IDLE`, `ERASING`, `RECEIVING`, `VERIFYING`, `COMPLETE`, `ABORTED`).
  - Covers boundary validations, staging memory containment, memory map overlap rejection, chunk alignments, sequence errors, and CRC32 verification.
- **Enhanced Security Gate & DID Test Coverage**:
  - Full branch coverage across lockout timings, zero-duration delays, ECU reset clearing, and NULL guards.
  - Complete DID handler fault injection, session masking, and security level permissions validation.
- **Hardware Abstraction Ports Integration (`ports/`)**:
  - Built and tested `uds_iso_tp_ports` library across STM32C0, STM32F1, STM32F4, STM32F7, and STM32G4 in both root cross-build and host test suite (`uds_iso_tp_ports_contract`).
- **Automated libFuzzer Execution in CI**:
  - Integrated 15s fuzzing runs in CI workflow executing `fuzz_isotp_rx` and `fuzz_uds_request` under AddressSanitizer and UndefinedBehaviorSanitizer.

### Fixed
- **Physical Validation Integrity**:
  - Reverted `board_profile.yaml` to truthful `ready-for-hardware` and `requires-selected-board` states to align with absence of attached physical bench harness in CI.
- **CI Quality Gates**:
  - Restored strict coverage thresholds to $\ge 90\%$ lines and $\ge 80\%$ branches after raising download and security gate coverage.

## [1.6.0] - 2026-09-28

### Added
- **Standard MCU Porting Architecture (`ports/`)**:
  - Defined 4 normalized hardware interfaces: `can_port.h`, `flash_port.h`, `clock_port.h`, and `reset_port.h`.
  - Added port implementations for STM32F7, STM32C0, STM32F1 (answering #71-#89), STM32F4, and STM32G4.
  - Published comprehensive [MCU Porting Guide](docs/porting_guide.md).
- **Pluggable Boot Integrity Policy (`BootIntegrityPolicy`, Fixes #68)**:
  - Modular image verification policies: `policy_crc32`, `policy_sha256`, and `policy_signature` (ECDSA/Ed25519).
- **Freestanding Cryptography Module (`library/crypto/sha256.h`)**:
  - Pure C99/C11 freestanding SHA-256 and HMAC-SHA256 validated against NIST CAVP and RFC 4231 vectors.
- **Security Attempt & Lockout Gate (`uds_security_gate.h`)**:
  - Independent security gate enforcing attempt thresholds, power-on delays, and seed reuse protection.
- **Non-Blocking Flash Range Erase & FSM Servicing**:
  - Sliced range erase (`Flash_RequestRangeErase`) enabling continuous UDS / ISO-TP flow control processing during 128 KiB flash erase cycles without timeout starvation.
- **Continuous Fuzzing Targets**:
  - libFuzzer test engines: `fuzz_isotp_rx` (CAN frames and PCI injection) and `fuzz_uds_request` (diagnostic request parser).
- **Process, Quality & CI Gates**:
  - Coverage gates: `--fail-under-line 90 --fail-under-branch 80`.
  - Cyclomatic complexity gate: `lizard -C 15` across `library/`.
  - MISRA C:2012 deviation record ([docs/misra_deviations.md](docs/misra_deviations.md)).
  - Requirements traceability matrix ([docs/iso_traceability.md](docs/iso_traceability.md)).
  - Hardware physical validation board profile ([docs/physical_validation/board_profile.yaml](docs/physical_validation/board_profile.yaml)).
  - GitHub issue templates for bug reports, feature requests, and porting support.

### Changed
- **Cyclomatic Complexity Reduction (CCN <= 15)**:
  - `uds_bootloader.c`: Monolithic routine control (CCN 39 -> 8) refactored into table dispatch `k_boot_routines`.
  - `uds_bootloader.c`: Application validation (CCN 16 -> 5) and flash programming refactored into modular helpers.
  - `uds_bootloader.c`: Hardware assembly vector jump isolated into `boot_jump.c`.
  - `uds.c`: Service dispatcher (CCN 44 -> 13) refactored to static table dispatch `k_service_dispatch_table`.
  - `uds.c`: Security access (CCN 24 -> 9) split into seed and key handlers.
  - `isotp.c`: Receive handler (CCN 32 -> 13) modularized into `rx_single`, `rx_first`, and `rx_consecutive`.
  - `uds_dtc_app.c`: Report switch (CCN 201 -> 13) refactored to table dispatch `k_dtc_subfns`.
- **Flash Programming Granularity (Fixes #91)**:
  - `UdsFlashPort` explicitly exposes `program_granule` (2, 4, 8, 16, 32 bytes) and `erased_byte`.
  - Wear-leveling metadata slots dynamically compute granule alignment with two-phase commit.
- **Single Source of Truth for DTC State & Versioning**:
  - Added `UDS_DTC_NV_MAGIC` (`0xD7C1U`) and `UDS_DTC_NV_VERSION` (`1U`) to NVM storage.
  - Unified DTC record mutations through atomic state management.
- **Cortex-M7 Concurrency Synchronization**:
  - Added Data Memory Barriers (`__DMB()`) around SPSC RX FIFO head/tail publications.
- **Release Versioning**:
  - Expose runtime version API `uds_iso_tp_version()` configured from root `VERSION` file.

### Fixed
- Fixed DID entry `const void *context_ro` preservation to prevent accidental writes through read-only context (`uds_did.c:40`).
- Added negative test verifying write (service 0x2E) to read-only DID returns NRC 0x31 (RequestOutOfRange).
- Added regression test `test_issue_65_clear_then_read_is_empty` verifying ClearDiagnosticInformation empties all subfunction lists.
- Fixed off-by-one and truncated request negative handling for multi-byte diagnostic lengths.

## [0.1.0] - 2026-08-15

### Added
- Bounded heap-free ISO-TP and UDS core APIs for Classical CAN and CAN FD.
- STM32F767 bxCAN and FDCAN-capable STM32 adapter contracts.
- Host tests, sanitizer validation, static analysis, HIL inventory tooling, and target build infrastructure.
- Documentation for architecture, transport profiles, UDS boundaries, STM32 integration, HIL, safety, and release readiness.
