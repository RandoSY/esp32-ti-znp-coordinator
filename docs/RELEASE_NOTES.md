# Release notes

## Experimental LQI investigation — 2026-10-18, completed diagnostics and separate checksum correction

- Reproduced bathroom zeros originate in the SDK APS field: all 11 raw zeros pass unchanged through bridge/ZNP/MQTT. Near raw values were 81..209 versus bathroom 0..15; all 63 near/bathroom/return-near values match end to end; return-near recovered to 173..209 without zeros. Genuine RF versus SDK metadata semantics remain undetermined; no measurement correction is justified.
- Encoder and installed host parser/publisher tests preserve zero and other boundary values. No measurement correction or cosmetic replacement introduced.
- Extracted the original clamp into a tested constexpr helper; default-OFF receive machine code and reported build resources match the baseline.
- Added optional `ZNP_LQI_DIAGNOSTICS` RAM-only metadata capture and read-only retrieval. OFF by default; no extra callback, payload logging or persistence writes.
- Added offline LQI regressions, metadata collectors and [validation/rollback instructions](LQI_Validation_Report.md). C6 builds in both modes; three-location metadata validation is complete.
- Following operator authorization, backed up host data and full flash, wrote only the diagnostic application, verified unchanged non-application storage, and resumed the existing network with unchanged host packages. Compatibility/release version unchanged; this is not a completed firmware release.

- Separately corrected valid-checksum FE decoder resynchronization. All 256 FCS values/recovery cases, protocol/host/Python tests and C6/H2 builds pass. Operator-approved C6 app-only deployment preserved non-app0 storage; live FE-boundary test and all six sensor reports from three cycles pass. Diagnostics OFF; no LQI replacement. [Full correction evidence](ZNP_Checksum_Boundary.md).

## Current development firmware — 2026-10-18

- Durable OSAL/NV_EXT, host-controlled formation/reset/resume and TI-compatible identity.
- Direct SDK backend with 15 coordinator endpoints, real groups, remote ZDO and APS delivery events.
- Fixed confirmations for copied ASDUs and concurrent distinct messages to one destination; regression coverage added.
- C6 UZ-6D interview/IAS enrollment and contact/battery publishing passed through full unmodified Zigbee2MQTT 2.14.2 and Mosquitto 2.1.2 on Windows.
- Operator confirmed green idle / white receive indication; color key and Python commands in Getting Started.
- Installed Windows applications with start/stop/contact-watch shortcuts. No Windows services or automatic login startup created.
- C6/H2 compile; 20 protocol and 12 NV checks, APS regressions and 10 Python tests pass; native host startup/resume passes unmodified herdsman 11.1.0.
- Full per-device TI security backup/restore and broader acceptance remain incomplete. Entries below describe historical firmware.

## Workspace validation and target clarification — 2026-10-17

- Confirmed target: impersonate a CC2652P's serial ZNP behavior for unmodified Zigbee2MQTT, backed by real ESP32 Zigbee operation.
- Compiled and linked the complete existing firmware for generic 4 MB ESP32-C6 and ESP32-H2 Dev Modules with Arduino-ESP32 3.3.11 and native USB CDC. Build settings and memory results are in `ARDUINO_SETUP.md`; artifacts are in `artifacts/esp32c6` and `artifacts/esp32h2`.
- Reran 20 native C++ checks (Windows GCC 15.2.0, without sanitizers) and five Python tests; all passed.
- Added VirtualBox / Ubuntu 22.04 USB setup, serial personality gap analysis against pinned zigbee-herdsman source, and separate hardware/host acceptance checks.
- No new ZNP emulation behavior added; no hardware flashed or RF / Zigbee2MQTT acceptance performed.

## v0.2.1 — 2026-10-17

- Integrated real Espressif Zigbee coordinator adapter from the multi-file v0.2.0 prototype.
- Ubuntu VM USB passthrough and Zigbee2MQTT isolated-instance operating guide.
- Numbered-menu, no-memorization guest console `tools/vm_menu.py`.
- APS transmission payload retained across asynchronous confirmations (avoids dangling source pointer).
- New Python read-only binary ZNP probe with serial discovery, transcript output and explicit command-response matching.
- New unit tests for incremental framing, FCS rejection, resynchronization and command reply parsing.
- Comprehensive command/help-based documentation; direct source layout and functional compatibility matrix.
- Native C++ test suite 20 checks; Python protocol probe 5 checks.

**Historical v0.2.1 limitation:** its startup/NV implementation was incomplete and had no physical acceptance. See the current entry above and [RF acceptance](RF_ACCEPTANCE.md) for subsequent implementation and hardware results.
