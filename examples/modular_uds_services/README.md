# Modular UDS Services Reference Implementation

This directory provides standalone, modular implementations of core ISO 14229-1 UDS diagnostic services, ISO 15765-2:2016 transport layer with CAN-FD support, and fault lifecycle management, designed for bare-metal targets (including STM32F103, STM32C0, STM32F7, RP2350):

| Service / Module | File | Subfunctions / Description |
|---|---|---|
| **0x19 ReadDTCInformation** | `svc_read_dtc.c` | `0x01` (count), `0x02` (by mask), `0x04` (snapshot DIDs: 0xDF00-0xDF04, 0xDD00), `0x06` (extended data: 0x01 occurrence, 0x02 aging), `0x0A` (supported DTCs) |
| **0x14 ClearDiagnosticInformation** | `svc_clear_dtc.c` | Clear all DTCs (`0xFFFFFF`), individual 24-bit DTCs, or group masks safely with ISO 14229-1 status reset |
| **0x85 ControlDTCSetting** | `svc_control_dtc.c` | `0x01` (on), `0x02` (off) with SPRMIB suppression |
| **0x28 CommunicationControl** | `svc_comm_control.c` | `0x00..0x03` (enable/disable Rx/Tx) for Normal and NM traffic |
| **0x23 ReadMemoryByAddress** | `svc_read_address.c` | ALFID decoding, 32-bit overflow check, address range protection |
| **0x3D WriteMemoryByAddress** | `svc_write_address.c` | ALFID decoding, SecurityAccess check, RAM range verification |
| **0x31 RoutineControl** | `svc_routine_control.c` | `0x01` (start), `0x02` (stop), `0x03` (results) with sequence validation |
| **Unified DTC Store** | `dtc_store.c`, `dtc_store.h` | FDC debouncing (-128..+127), driving cycle unlearning/aging, snapshot & extended data packing |
| **85 Standard DTCs** | `dtc_table_85.c`, `dtc_table_85.h` | All 85 DTCs extracted from specification (`DTC.uds.xlsx`) |
| **ISO 15765-2 CAN-FD** | `isotp_fd.c`, `isotp_fd.h`, `can_frame_fd.h` | 64-byte CAN-FD support: SF (up to 62 bytes), standard & extended 32-bit First Frame, CF, FC |
| **UDS Server Dispatcher** | `uds_server_dispatch.c`, `uds_server_dispatch.h` | Addressing modes (Physical / Functional) and security level masks |
| **Lifecycle Example** | `dtc_lifecycle_example.c` | Complete end-to-end usage example for Ignition ON, sensor task, and Ignition OFF |

## Features & Standards Conformance
- **ISO 14229-1:2013 / 2020** Table A.1 Negative Response Code (NRC) precedence.
- **ISO 15765-2:2016** CAN-FD transport layer specification with payload lengths up to 64 bytes.
- **SPRMIB Handling**: Masking bit 7 (`0x80`) of subfunctions and suppressing positive responses properly.
- **Buffer Safety**: All responses verify output capacity against `ISOTP_TX_BUF_SIZE` to prevent memory corruption.
- **MISRA C & Zero-Warning**: Typedefs, strict unsigned constant literals (`U`), bounds checks, and zero warnings under strict compiler flags.
