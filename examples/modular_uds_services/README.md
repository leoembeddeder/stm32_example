# Modular UDS Services Reference Implementation

This directory provides standalone, modular implementations of core ISO 14229-1 UDS diagnostic services, designed for bare-metal STM32 targets (such as STM32F103, STM32C0, STM32F7):

| Service | File | Subfunctions / Description |
|---|---|---|
| **0x19 ReadDTCInformation** | `svc_read_dtc.c` | `0x01` (count), `0x02` (by mask), `0x04` (snapshot), `0x06` (extended data), `0x0A` (supported DTCs) |
| **0x85 ControlDTCSetting** | `svc_control_dtc.c` | `0x01` (on), `0x02` (off) with SPRMIB suppression |
| **0x28 CommunicationControl** | `svc_comm_control.c` | `0x00..0x03` (enable/disable Rx/Tx) for Normal and NM traffic |
| **0x23 ReadMemoryByAddress** | `svc_read_address.c` | ALFID decoding, 32-bit overflow check, address range protection |
| **0x3D WriteMemoryByAddress** | `svc_write_address.c` | ALFID decoding, SecurityAccess check, RAM range verification |
| **0x31 RoutineControl** | `svc_routine_control.c` | `0x01` (start), `0x02` (stop), `0x03` (results) with sequence validation |

## Features & Standards Conformance
- **ISO 14229-1:2013 / 2020** Table A.1 Negative Response Code (NRC) precedence.
- **SPRMIB Handling**: Masking bit 7 (`0x80`) of subfunctions and suppressing positive responses properly.
- **Buffer Safety**: All responses verify output capacity against `ISOTP_TX_BUF_SIZE` to prevent memory corruption.
