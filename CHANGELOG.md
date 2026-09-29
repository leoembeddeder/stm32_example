# Changelog

All notable changes to this project are documented here. Host validation and target compatibility remain dependent on the exact compiler, MCU, HAL revision, transceiver, and board configuration.

## [1.6.2] - 2026-09-29

### Security
- **P0-1 Cryptographic Verification Fail-Closed (`boot_verify.c`)**:
  - Removed hardcoded mock HMAC key from production builds.
  - Implemented fail-closed signature verification policy requiring an explicit registered cryptographic verifier callback.
  - Replaced variable-time `memcmp` with constant-time equality check `constant_time_equal()` for message authentication checks.
- **P0-2 SecurityAccess Provisioning Enforcement (`uds_security_app.c`)**:
  - Removed fallback NIST reference key from non-test builds.
  - Added `uds_security_app_is_provisioned()` checking option bytes/NVM keys.
  - Refused Level 2 seed requests on unprovisioned devices with `UDS_RESULT_DENIED` (NRC 0x22 / ConditionsNotCorrect).
- **P0-3 Lockout State & Reset Bypass Elimination (`uds_security_gate.c`, `uds.c`)**:
  - Unified `UdsSecurityGate` and `UdsServer`, eliminating duplicate security logic.
  - Ensured failed attempt counter and lockout active state survive `0x11` ECU reset and power cycles.
  - Enforced non-zero minimum lockout delay (clamped to 10,000 ms).
  - Added persistence callback interface `uds_server_set_security_persistence()` and state restoration `uds_server_restore_security_state()`.
- **P0-4 Monotonic Anti-Rollback Floor Restoration (`uds_bootloader.c`)**:
  - Restored active firmware version floor from validated NVM metadata at boot.
  - Added CRC-32 header integrity validation and format version checks before image acceptance or activation.
- **P0-5 Fail-Closed Entropy Generation (`uds_security_app.c`)**:
  - Refused seed generation when no true hardware TRNG or hardware entropy source is available.
  - Propagated all entropy errors and removed discarded `(void)` return value casts.

### Standards Conformance
- **P1-1 ISO 15765-2 Reserved STmin Conformance (`isotp.c`)**:
  - Mapped reserved STmin values (`0x80–0xF0`, `0xFA–0xFF`) to 127 ms (127,000 µs) per ISO 15765-2 Section 9.6.5.4 instead of aborting flow control.
- **P1-3 ISO 15765-2 Classic CAN Frame Length Enforcement (`isotp.c`)**:
  - Enforced full DLC (8 bytes) on Classic CAN First Frame (`ISOTP_ERR_FORMAT` if DLC < 8).
  - Enforced full DLC (8 bytes) on all non-final Consecutive Frames on Classic CAN.
- **P1-4 Metadata Format & CRC-32 Validation (`boot_verify.h`, `boot_verify.c`)**:
  - Added `format_version`, `flags`, and `header_crc32` to `FirmwareMetadata_t` with 128-byte packed footprint preservation.
- **P2-4 Unified Error Model (`uds.h`, `uds.c`)**:
  - Exposed `uds_result_to_nrc()` with contract tests walking all `UdsCallbackResult` enum values.

### Architecture & Verification
- **P2-3 MCU Port Headers & Strict Warnings (`ports/`)**:
  - Included `ports.h` across all 5 STM32 port files, resolving missing prototype warnings.
  - Added `-Wmissing-prototypes` and `-Werror` flags to `uds_iso_tp_ports`.
- **P3-1 Deep libFuzzer Penetration (`fuzz_uds_request.c`)**:
  - Equipped UDS request fuzzer with in-memory service callbacks for DID read/write, security seed/key, routine control, and download/transfer.
- **P3-3 CI Coverage HTML Artifact (`standalone-uds.yml`)**:
  - Configured CI workflow to generate and upload `coverage-report` HTML artifact.
- **P3-4 Static Analysis (`uds_wear_leveling.c`)**:
  - Eliminated GCC analyzer uninitialized buffer note with zero-initialized staging chunk.

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
