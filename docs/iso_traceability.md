# ISO 14229-1 & ISO 15765-2 Requirements Traceability Matrix

This matrix establishes formal traceability between international diagnostic and transport standards, implementation functions, automated unit/integration tests, and hardware bench evidence.

| Standard & Clause | Service Name / Feature | Implementation Function | Test Contract | Hardware Verification Evidence |
|-------------------|------------------------|-------------------------|---------------|--------------------------------|
| **ISO 15765-2: §9.2** | Single Frame (SF) Transmission & Reception | `rx_single`, `tx_build_sf` (`isotp.c`) | `test_isotp.c` (`uds_iso_tp_isotp_contract`) | CAN analyzer trace `CAN_TRC_SF_01.pcap` |
| **ISO 15765-2: §9.3** | First Frame (FF) & Segmentation | `rx_first`, `tx_build_first_frame` (`isotp.c`) | `test_isotp.c` (`uds_iso_tp_isotp_contract`) | Oscilloscope capture `ISO_FF_FLOW_02.png` |
| **ISO 15765-2: §9.4** | Flow Control (FC) & STmin Timing | `isotp_tx_feed_flow_control` (`isotp.c`) | `test_isotp.c`, `test_adapters.c` | Logic analyzer STmin 0..127ms validation |
| **ISO 15765-2: §9.5** | Consecutive Frame (CF) Sequencing | `rx_consecutive`, `isotp_tx_next` (`isotp.c`) | `test_isotp.c` (`uds_iso_tp_isotp_contract`) | Zero packet drop under 90% bus load |
| **ISO 14229-1: §9.2** | 0x10 DiagnosticSessionControl | `service_session_control` (`uds.c`) | `test_session_security.c`, `test_services.c` | Bench trace `UDS_0x10_SESSION.log` |
| **ISO 14229-1: §9.3** | 0x11 ECUReset (Hard, Soft, Shutdown) | `service_ecu_reset` (`uds.c`) | `test_reset_recovery.c`, `test_c092_app_shim.c` | Rapid power reset recovery bench test |
| **ISO 14229-1: §11.2**| 0x14 ClearDiagnosticInformation | `service_clear_dtc` (`uds.c`, `uds_dtc_app.c`)| `test_bootloader_dtc_app.c` (`test_issue_65`) | NVM flash clear bench verification |
| **ISO 14229-1: §11.3**| 0x19 ReadDTCInformation | Table-driven `k_dtc_subfns` (`uds_dtc_app.c`) | `test_dtc.c`, `test_bootloader_dtc_app.c` | Subfunction 0x01, 0x02, 0x04, 0x06 reports |
| **ISO 14229-1: §10.2**| 0x22 ReadDataByIdentifier | `service_read_did` (`uds.c`, `uds_did.c`) | `test_did_security_auth_app.c` | Read VIN, Sw/Hw versions via PCAN |
| **ISO 14229-1: §9.4** | 0x27 SecurityAccess (Multi-level Seed/Key)| `service_security_seed`, `service_security_key`, `uds_security_gate.c` | `test_multilevel_security.c`, `test_security_gate.c` | Security lockout and delay timer bench verification |
| **ISO 14229-1: §10.3**| 0x2E WriteDataByIdentifier | `service_write_did` (`uds.c`, `uds_did.c`) | `test_uds.c` (`uds_iso_tp_uds_contract`) | Write VIN, read-only DID rejection (NRC 0x31) |
| **ISO 14229-1: §12.2**| 0x31 RoutineControl (Erase, CheckMemory) | `uds_bootloader_routine_control` (`uds_bootloader.c`) | `test_c092_bootloader.c`, `test_bootloader_dtc_app.c` | Erase slot B and SHA-256 verify bench logs |
| **ISO 14229-1: §14.2**| 0x34 RequestDownload | `service_request_download` (`uds.c`, `uds_download.c`) | `test_c092_bootloader.c` | A/B staging flash memory allocation |
| **ISO 14229-1: §14.3**| 0x36 TransferData | `service_transfer_data` (`uds.c`, `uds_download.c`)| `test_c092_bootloader.c` | Bounded chunk block transfer sequence |
| **ISO 14229-1: §14.4**| 0x37 RequestTransferExit | `service_request_transfer_exit` (`uds.c`)| `test_c092_bootloader.c` | CRC32 & SHA-256 validation before flash commit |
| **ISO 14229-1: §14.5**| 0x38 RequestFileTransfer | `service_request_file_transfer` (`uds.c`)| `test_services_extended.c` | File transfer protocol conformance |
| **ISO 14229-1: §10.4**| 0x2F InputOutputControlByIdentifier | `service_io_control` (`uds.c`, `uds_io_control_app.c`)| `test_services_extended.c` | ShortTermAdjustment and ReturnControl |
| **ISO 14229-1: §9.6** | 0x85 ControlDTCSetting | `service_control_dtc_setting` (`uds.c`) | `test_bootloader_dtc_app.c` | DTC update freeze during flashing |
| **ISO 14229-1: §9.7** | 0x86 ResponseOnEvent | `service_response_on_event` (`uds.c`, `uds_roe_app.c`)| `test_services_extended.c` | Asynchronous event telemetry report |
