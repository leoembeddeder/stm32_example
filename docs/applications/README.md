# Automotive Diagnostic Applications & Examples

This directory provides architecture recommendations, hardware schematics, and software examples for turning the **STM32 UDS ISO-TP** stack into production-grade automotive diagnostic devices, scan tools, and testing equipment.

---

## Application Categories

| Application Profile | Target Hardware | Primary Use Case | Guide |
| :--- | :--- | :--- | :--- |
| **Handheld Diagnostic Scan Tool** | STM32 + SPI/Parallel LCD + Keypad | Standalone code reader (read/clear DTCs, live sensor display, OBD-II/UDS inspection). | [Diagnostic Device Guide](diagnostic_device_guide.md#option-a-standalone-handheld-scan-tool) |
| **Smart Wireless OBD-II Dongle** | STM32 + BLE/Wi-Fi Coprocessor (ESP32/nRF52) | Connects to iOS/Android mobile apps for real-time dashboard telemetry, fault code clearing, and cloud logging. | [Diagnostic Device Guide](diagnostic_device_guide.md#option-b-smart-wireless-obd-ii-dongle) |
| **USB Pass-Thru / Flashing Interface** | STM32 with USB FS/HS (CDC / J2534 / WinUSB) | PC-based ECU flashing, calibration, CANoe/Wireshark/SavvyCAN logging, and tuning. | [Diagnostic Device Guide](diagnostic_device_guide.md#option-c-usb-diagnostic--tuning-interface) |
| **Dual-Board ECU Simulator & Bench Tester** | Two interconnected STM32 boards | Golden ECU reference bench to validate third-party scanners, telematics devices, and loggers. | [Diagnostic Device Guide](diagnostic_device_guide.md#6-bench-testing-without-a-real-vehicle-dual-board-setup) |

---

## Key Modules Reused from this Repository

When developing a diagnostic client (tester) using this project, you leverage the following core modules directly:

1. **ISO 15765-2 Transport Engine** ([`library/src/isotp.c`](../../library/src/isotp.c)):
   - Handles multi-frame packet reassembly, flow control negotiation, microsecond/millisecond $ST_{min}$ pacing, and CAN FD 64-byte payloads.
2. **Lock-Free RX Circular Queue** ([`App/Src/uds_app.c`](../../App/Src/uds_app.c)):
   - Absorbs high-speed CAN bursts with $<5\,\mu\text{s}$ ISR latency, eliminating frame loss during high bus loads.
3. **Hardware TX Mailbox Backpressure** ([`App/Src/can_transport.c`](../../App/Src/can_transport.c)):
   - Prevents buffer overflow when transmitting back-to-back Consecutive Frames.
4. **Diagnostic Trouble Code (DTC) Parser** ([`library/src/uds_dtc.c`](../../library/src/uds_dtc.c)):
   - Decodes 3-byte DTC numbers, freeze frames, and AUTOSAR Dem status masks into standard SAE J2012 codes (`P`, `C`, `B`, `U`).
5. **RFC 4493 AES-CMAC-128 Engine** ([`library/crypto/aes_cmac.c`](../../library/crypto/aes_cmac.c)):
   - Provides constant-time cryptographic key derivation for unlocking OEM ECUs during Service `0x27` SecurityAccess.

---

## Detailed Guides

* [**Automotive Diagnostic Device Implementation Guide**](diagnostic_device_guide.md): Complete electrical protection schematics, J1962 pinout, software client implementation (`uds_diagnostic_client.c`), live sensor formulas, and dual-board bench validation.
