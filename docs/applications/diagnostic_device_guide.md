# Engineering Guide: Building an Automotive Diagnostic Device with STM32 UDS ISO-TP

This engineering guide explains how to leverage the **STM32 UDS ISO-TP** repository to create a production-grade automotive diagnostic device—such as a handheld scan tool, a wireless OBD-II dongle, or a USB ECU reflashing interface.

---

## 1. System Architecture: Client (Tester) vs. Server (ECU)

Automotive diagnostics operate under a client-server paradigm specified by ISO 14229-1 (UDS) and ISO 15765-2 (ISO-TP):

* **Diagnostic Server (ECU)**: Installed inside the vehicle (e.g., Engine Control Module, ABS, Body Controller). It listens for requests on physical request CAN IDs (e.g., `0x7E0`) or the broadcast functional ID (`0x7DF`) and sends responses on response IDs (e.g., `0x7E8`).
* **Diagnostic Client (Tester / Scan Tool)**: The external tool plugged into the 16-pin OBD-II port. It initiates requests (queries DTCs, reads sensor parameters, commands actuator tests, or flashes firmware) and processes responses.

```
+-----------------------------------------------------------------------------------+
|                            AUTOMOTIVE DIAGNOSTIC SYSTEM                           |
|                                                                                   |
|    +-----------------------------+               +---------------------------+    |
|    |      Diagnostic Device      |  CAN Bus      |    Vehicle ECU (ECM)      |    |
|    |      (Tester / Client)      |  (500 kbps)   |    (Diagnostic Server)    |    |
|    +-----------------------------+               +---------------------------+    |
|    |  App: Scan Tool / Screen    |               |  App: Vehicle Engine Ctrl |    |
|    |  Client State Machine       |               |  UDS Server (uds.c)       |    |
|    |  ISO-TP TX (0x7DF / 0x7E0)  | ------------> |  ISO-TP RX (0x7DF / 0x7E0)|    |
|    |  ISO-TP RX (0x7E8 - 0x7EF)  | <------------ |  ISO-TP TX (0x7E8)        |    |
|    |  CAN Hardware Driver (HAL)  |               |  can_transport.c / HAL    |    |
|    +-----------------------------+               +---------------------------+    |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 2. Automotive Hardware & Electrical Interface

Connecting to a vehicle's On-Board Diagnostics port requires adhering to **SAE J1962** and protecting the microcontroller from harsh automotive electrical transients (**ISO 7637-2**).

### A. 16-Pin OBD-II (J1962) Pinout

```text
       1   2   3   4   5   6   7   8
     +-------------------------------+
      \  .   .   .   G   G   H   .   . /
       \ .   .   .   .   .   L   .   B/
        +---------------------------+
          9  10  11  12  13  14  15  16
```

| Pin | Designation | Connection on Diagnostic Device |
| :---: | :--- | :--- |
| **4** | Chassis Ground | Connect to System Ground |
| **5** | Signal Ground | Connect to System Ground |
| **6** | CAN High (ISO 15765-4) | Connect to CAN Transceiver `CANH` |
| **14** | CAN Low (ISO 15765-4) | Connect to CAN Transceiver `CANL` |
| **16** | Battery Power ($+12\text{V} / +24\text{V}$) | Connect to Input of Automotive Step-Down Regulator |
| *Others* | Vendor-specific / K-Line | Leave unconnected for pure CAN/UDS tools |

### B. Electrical Schematic & Protection Blueprint

Automotive power nets experience reverse polarity, inductive voltage spikes, and alternator load dumps (up to $+40\text{V}$ on $12\text{V}$ cars per ISO 7637-2 Test Pulse 5b).

```text
OBD Pin 16 (+12V) ----[ PTC 500mA ]----+----[ P-MOSFET / Diode ]----+----[ Auto Buck Step-Down ]----+----> +3.3V (STM32)
                                       |      (Reverse Polarity)     |     (LM5017 / MP2315)
                                    [ TVS ]                       [ CAP ]
                                  (SMAJ24CA)                     (100uF 50V)
                                       |                             |
OBD Pin 4/5 (GND) ---------------------+-----------------------------+-------------------------------> GND

OBD Pin 6 (CAN_H) ----+----[ PESD2CAN TVS ]----+----[ Optional 120R Switch ]----+----> CANH (TJA1051T/3)
                      |                        |                                |
OBD Pin 14 (CAN_L) ---+                        +--------------------------------+----> CANL (TJA1051T/3)
```

1. **Overvoltage & Load Dump Protection**:
   * A $500\,\text{mA}$ resettable PTC fuse in series with Pin 16.
   * A bidirectional **SMAJ24CA** TVS diode (or SMBJ30CA for $24\text{V}$ tolerance) across $+12\text{V}$ and GND.
   * A P-channel MOSFET (e.g. AO3401A) or Schottky diode for reverse polarity protection.
2. **Step-Down Regulator**:
   * Wide input range ($6\text{V}$ to $40\text{V}$) switching buck converter (e.g., TI LM5017, Monolithic Power MP2315, or ST L4978) generating $+5\text{V}$ or $+3.3\text{V}$.
3. **CAN Bus Transceiver**:
   * Classical CAN: **NXP TJA1051T/3** or **Microchip MCP2562** (has internal level shifter for $3.3\text{V}$ MCU I/O).
   * CAN FD: **Texas Instruments TCAN1042-Q1** or **NXP TJA1044GT**.
   * TVS Diode: **NXP PESD2CAN** or **ON Semi NUP2105L** directly at the connector pins.
4. **Termination Resistor**:
   * Vehicles already have two $120\,\Omega$ termination resistors installed at opposite ends of the wiring harness ($60\,\Omega$ equivalent). Your diagnostic tool must **not** have a permanent $120\,\Omega$ termination connected when plugged into a full vehicle. Include a DIP switch or solder jumper for bench testing.

---

## 3. Reusing Core Modules for Diagnostic Client

The modules in this repository provide everything needed to implement a client:

| Repository Module | File Location | Client Application Role |
| :--- | :--- | :--- |
| **ISO-TP Link Engine** | [`library/src/isotp_link.c`](../../library/src/isotp_link.c) | Assembles outgoing diagnostic requests and reassembles multi-frame ECU responses. |
| **Lock-Free RX FIFO** | [`App/Src/uds_app.c`](../../App/Src/uds_app.c) | Receives incoming CAN frames in interrupt context without dropping packets. |
| **Hardware TX Mailbox**| [`App/Src/can_transport.c`](../../App/Src/can_transport.c) | Paces multi-frame transmissions according to hardware mailbox availability. |
| **DTC Definitions** | [`library/include/uds_iso_tp/uds_dtc.h`](../../library/include/uds_iso_tp/uds_dtc.h) | Parses 3-byte DTC numbers, freeze frame records, and 8-bit status masks. |
| **RFC 4493 AES-CMAC** | [`library/crypto/aes_cmac.c`](../../library/crypto/aes_cmac.c) | Computes cryptographic responses to unlock ECUs during Service `0x27`. |

---

## 4. Complete Application Example: `uds_diagnostic_client.c`

Below is a complete, production-grade diagnostic client implementation demonstrating how to read vehicle VIN, scan DTCs, clear faults, and read live OBD-II engine telemetry using the repository's core libraries:

```c
/**
 * @file uds_diagnostic_client.c
 * @brief Automotive Diagnostic Tester (Client) Implementation
 */

#include "uds_iso_tp/isotp_link.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Standard ISO 15765-4 CAN Diagnostic Identifiers */
#define CAN_ID_FUNCTIONAL_REQ    0x7DFU  /* Broadcast request to all vehicle ECUs */
#define CAN_ID_PHYSICAL_REQ_ENG  0x7E0U  /* Physical request to Engine Control Module */
#define CAN_ID_PHYSICAL_RESP_ENG 0x7E8U  /* Physical response from Engine Control Module */

/* Buffer sizing */
#define CLIENT_BUFFER_SIZE       512U

static IsoTpLink g_diag_link;
static uint8_t   g_client_tx_buffer[CLIENT_BUFFER_SIZE];
static uint8_t   g_client_rx_buffer[CLIENT_BUFFER_SIZE];

/**
 * @brief Initialize the Diagnostic Client ISO-TP Link
 */
void diag_client_init(void) {
    isotp_link_init(&g_diag_link,
                    CAN_ID_PHYSICAL_REQ_ENG,
                    g_client_tx_buffer,
                    sizeof(g_client_tx_buffer),
                    g_client_rx_buffer,
                    sizeof(g_client_rx_buffer));
}

/**
 * @brief Send a request to read the Vehicle Identification Number (VIN)
 * Service: 0x22 (ReadDataByIdentifier), DID: 0xF190
 */
bool diag_client_request_vin(void) {
    static const uint8_t vin_req[] = { 0x22, 0xF1, 0x90 };
    return (isotp_send(&g_diag_link, vin_req, sizeof(vin_req)) == ISOTP_RET_OK);
}

/**
 * @brief Send a request to read all active and confirmed Fault Codes (DTCs)
 * Service: 0x19 (ReadDTCInformation), Subfunction: 0x02 (reportDTCByStatusMask)
 * Status Mask: 0x09 (TestFailed | ConfirmedDTC)
 */
bool diag_client_request_dtcs(void) {
    static const uint8_t dtc_req[] = { 0x19, 0x02, 0x09 };
    return (isotp_send(&g_diag_link, dtc_req, sizeof(dtc_req)) == ISOTP_RET_OK);
}

/**
 * @brief Send a request to clear all fault codes
 * Service: 0x14 (ClearDiagnosticInformation), Group: 0xFFFFFF (All DTCs)
 */
bool diag_client_clear_all_dtcs(void) {
    static const uint8_t clear_req[] = { 0x14, 0xFF, 0xFF, 0xFF };
    return (isotp_send(&g_diag_link, clear_req, sizeof(clear_req)) == ISOTP_RET_OK);
}

/**
 * @brief Send a request for live engine telemetry (OBD-II Mode 01)
 * @param pid 0x0C = Engine RPM, 0x0D = Vehicle Speed, 0x05 = Engine Coolant Temp
 */
bool diag_client_request_live_pid(uint8_t pid) {
    uint8_t pid_req[2];
    pid_req[0] = 0x01; /* OBD-II Mode 01: Current Powertrain Diagnostic Data */
    pid_req[1] = pid;
    return (isotp_send(&g_diag_link, pid_req, sizeof(pid_req)) == ISOTP_RET_OK);
}

/**
 * @brief Decode standard SAE J2012 3-byte DTC into human-readable text
 * e.g., 0x0100 -> P0100, 0xC001 -> U0001
 */
static void decode_dtc_string(uint32_t dtc_raw, char *out_str, size_t out_size) {
    static const char prefix[4] = { 'P', 'C', 'B', 'U' };
    uint8_t type_idx = (uint8_t)((dtc_raw >> 22) & 0x03);
    uint32_t code_num = (dtc_raw >> 8) & 0x3FFF;
    uint8_t failure_type = (uint8_t)(dtc_raw & 0xFF);

    if (failure_type != 0x00) {
        snprintf(out_str, out_size, "%c%04X-%02X", prefix[type_idx], code_num, failure_type);
    } else {
        snprintf(out_str, out_size, "%c%04X", prefix[type_idx], code_num);
    }
}

/**
 * @brief Dispatcher for incoming diagnostic responses from vehicle ECUs
 */
void diag_client_on_response_received(const uint8_t *data, uint16_t length) {
    if (length == 0) return;

    uint8_t response_sid = data[0];

    /* 1. Check for Negative Response Code (NRC 0x7F) */
    if (response_sid == 0x7FU && length >= 3) {
        uint8_t rejected_sid = data[1];
        uint8_t nrc_code = data[2];
        printf("[SCANNER] Negative Response! SID: 0x%02X, NRC: 0x%02X\n", rejected_sid, nrc_code);
        return;
    }

    /* 2. Process Positive Responses */
    switch (response_sid) {
        case 0x62: { /* Response to 0x22 (ReadDataByIdentifier) */
            uint16_t did = (uint16_t)((data[1] << 8) | data[2]);
            if (did == 0xF190 && length >= 20) {
                char vin[18] = { 0 };
                memcpy(vin, &data[3], 17);
                printf("[SCANNER] VIN Identified: %s\n", vin);
            }
            break;
        }

        case 0x59: { /* Response to 0x19 (ReadDTCInformation) */
            uint8_t subfunction = data[1];
            uint8_t status_availability_mask = data[2];
            uint16_t dtc_count = (length - 3) / 4;
            printf("[SCANNER] Found %u Fault Codes (Mask: 0x%02X):\n", dtc_count, status_availability_mask);

            for (uint16_t i = 0; i < dtc_count; ++i) {
                const uint8_t *record = &data[3 + (i * 4)];
                uint32_t dtc_raw = ((uint32_t)record[0] << 16) |
                                   ((uint32_t)record[1] << 8)  |
                                   ((uint32_t)record[2]);
                uint8_t status_byte = record[3];

                char dtc_name[16];
                decode_dtc_string(dtc_raw, dtc_name, sizeof(dtc_name));
                printf("   [%u] %s (Status: 0x%02X - %s%s)\n",
                       i + 1, dtc_name, status_byte,
                       (status_byte & 0x08) ? "CONFIRMED " : "",
                       (status_byte & 0x01) ? "ACTIVE" : "HISTORY");
            }
            break;
        }

        case 0x54: { /* Response to 0x14 (ClearDiagnosticInformation) */
            printf("[SCANNER] SUCCESS: Vehicle Fault Memory Cleared!\n");
            break;
        }

        case 0x41: { /* Response to OBD-II Mode 01 (Live Telemetry) */
            uint8_t pid = data[1];
            if (pid == 0x0C && length >= 4) { /* Engine RPM */
                uint16_t rpm_raw = (uint16_t)((data[2] << 8) | data[3]);
                float rpm = (float)rpm_raw / 4.0f;
                printf("[SCANNER] Engine Speed: %.1f RPM\n", rpm);
            } else if (pid == 0x0D && length >= 3) { /* Vehicle Speed */
                uint8_t speed_kmh = data[2];
                printf("[SCANNER] Vehicle Speed: %u km/h\n", speed_kmh);
            } else if (pid == 0x05 && length >= 3) { /* Coolant Temperature */
                int16_t temp_c = (int16_t)data[2] - 40;
                printf("[SCANNER] Engine Coolant: %d deg C\n", temp_c);
            }
            break;
        }

        default:
            printf("[SCANNER] Unhandled Positive Response SID: 0x%02X\n", response_sid);
            break;
    }
}
```

---

## 5. Product Form Factors & Connectivity Options

### Option A: Standalone Handheld Scan Tool
* **Hardware**: STM32F767 / STM32G4 / STM32F4 + 2.8" SPI TFT LCD (ST7789 or ILI9341) + 4 tactile navigation buttons (Up, Down, Enter, Back).
* **Firmware Flow**:
  1. User presses "Read Codes" $\rightarrow$ triggers `diag_client_request_dtcs()`.
  2. Display lists all retrieved DTCs with descriptions looked up from an internal flash table.
  3. User presses "Clear Codes" $\rightarrow$ triggers `diag_client_clear_all_dtcs()`.

### Option B: Smart Wireless OBD-II Dongle
* **Hardware**: STM32 connected via high-speed UART ($921,600\,\text{baud}$) to an ESP32-C3 or Nordic nRF52840 BLE module.
* **Firmware Flow**:
  * Emulates standard ELM327 AT commands (`ATZ`, `ATSP6`, `0100`, `22F190`) or uses a fast binary protobuf framing.
  * Connects wirelessly to smartphone apps (Torque Pro, Car Scanner ELM, or a custom Flutter/React Native app).

### Option C: USB Diagnostic & Tuning Interface
* **Hardware**: STM32 with USB Full-Speed / High-Speed interface.
* **Firmware Flow**:
  * Implements USB CDC (Virtual COM Port) or WinUSB / libusb.
  * Compatible with open-source automotive tools like **SavvyCAN**, **Wireshark** (via extcap), or custom Python scripts using `python-can` and `udsoncan`.

---

## 6. Bench Testing Without a Real Vehicle: Dual-Board Setup

You can develop, test, and calibrate your diagnostic tool on your desk without connecting to a car by setting up two STM32 boards back-to-back:

```text
+-----------------------+                         +-----------------------+
|     BOARD A: ECU      |                         |   BOARD B: SCANNER    |
|   (Diagnostic Server) |                         |  (Diagnostic Client)  |
|                       |      CAN High           |                       |
| STM32F767 / STM32C092 |-------------------------| STM32 Development Brd |
|   (Current Repo Code) |      CAN Low            |   (diag_client.c)     |
|                       |-------------------------|                       |
|   Transceiver CANH/L  |  [ 120 Ohm Resistor ]   |   Transceiver CANH/L  |
+-----------------------+                         +-----------------------+
```

### Verification Steps:
1. **Flash Board A** with the default repository build (acts as ECU Server with ID `0x7E0`, response `0x7E8`, DTC table, and VIN `0xF190`).
2. **Flash Board B** with the client firmware containing `uds_diagnostic_client.c`.
3. Connect `CAN_H` to `CAN_H`, `CAN_L` to `CAN_L`, and place a $120\,\Omega$ resistor across the lines.
4. On Board B, execute `diag_client_request_vin()`:
   * Board A receives `22 F1 90` and replies with `62 F1 90 <17-byte VIN>`.
   * Board B parses and prints the vehicle VIN.
5. On Board B, execute `diag_client_request_dtcs()`:
   * Board A replies with its active DTC list.
6. On Board B, execute `diag_client_clear_all_dtcs()`:
   * Board A clears fault records, resets status to `0x50` (AUTOSAR Dem), and responds with `54`.

---

## 7. Vehicle Safety & Best Practices

When deploying your device on live vehicles, adhere strictly to these rules:

1. **Never Send Flashing Services on Functional ID (`0x7DF`)**:
   * Services `0x34` (RequestDownload), `0x36` (TransferData), and `0x31 0xFF00` (EraseFlash) must **only** be addressed physically to the target ECU. A functional broadcast could disrupt multiple controllers simultaneously.
2. **Handle Diagnostic Session Keep-Alive ($P2$ / $P2^*$ Timers)**:
   * If keeping an ECU in Extended Diagnostic Session (`0x10 0x03`) or Programming Session (`0x10 0x02`), the diagnostic tool must transmit **TesterPresent** (`3E 80`) periodically every $2.0$ to $4.0$ seconds to prevent the ECU from reverting to the Default session.
3. **Bus-Off Management**:
   * If CAN errors exceed thresholds, the STM32 CAN controller enters the **Bus-Off** state. The firmware must catch `HAL_CAN_ErrorCallback()` and perform an automatic recovery sequence without requiring a hard power cycle.
