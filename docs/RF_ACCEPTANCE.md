# RF and host acceptance

The ESP32 emulates TI's serial host interface, not TI's CPU firmware or a particular board's RF amplifier. Its native 802.15.4 radio forms the real Zigbee network. USB carries binary ZNP frames directly or through a UART bridge.

## Recorded evidence: 2026-10-18

| Test | Result |
|---|---|
| C6 firmware build, initial full erase/program, subsequent app updates | Passed on WeAct ESP32-C6FH4, revision 0.2, 4 MB, native USB COM17 |
| H2 firmware compile/link | Passed with Arduino-ESP32 3.3.11; no H2 hardware test |
| Native protocol/NV tests and Python parser/transaction tests | Passed; simulated radio only |
| Unmodified zigbee-herdsman 11.1.0 | Adapter startup, reset/resume, coordinator descriptors, and Controller.start passed on C6 |
| Physical UZ-6D door/window sensor | Identified Wing TS0203, EndDevice; successful interview and IAS enrollment |
| Real APS TX/RX | Successful AF confirmations, attribute reads/writes, incoming contact and battery reports |
| Full unmodified Zigbee2MQTT 2.14.2 | Passed Windows startup/resume with bundled herdsman 10.10.0, TI ZStack3x0 personality |
| Mosquitto 2.1.2 | Connected on localhost, bridge online, contact true/false and battery messages received over MQTT |
| Reflash/reboot persistence | Saved network and interviewed sensor communicated after app update and restart without pairing again |
| RGB receive indication | Operator confirmed green at idle and brief white flashes with reports |
| Ubuntu 22.04 VirtualBox USB/application path | Instructions prepared; actual guest path not yet tested |
| TI backup migration, total power-loss fault injection, leave/rejoin cycles, multiple sensor/router families | Not yet accepted |

`contact: true` means closed, `false` means open. Host startup, RF reports, MQTT conversion, persistence, and migration are separate results. One sensor test does not establish all-device compatibility or certification.

Private logs, the host database, network configuration, build metadata/hashes, and raw transcripts are kept under ignored `artifacts/` and `lab-zigbee2mqtt/`. Do not publish these unreviewed: startup/debug traces may contain network keys.

## Remaining acceptance work

Follow the prioritized [go-forward test plan](GO_FORWARD.md). It defines restart/power-cycle, broader device/routing, leave/rejoin, overnight operation and Ubuntu/VirtualBox acceptance, plus the implementation gate for complete TI security backup/restore. Planned checks remain unpassed until evidence is recorded.

Compare packet traces against a real CC2652P on the same host release; exercise more devices, routers, concurrent requests, leave/rejoin, channel/key changes, and repeated power cycles. Implement and validate per-device security table translation before claiming TI coordinator backup/restore equivalence. Run the Ubuntu guest USB path separately. See [testing](TESTING.md) for the repeatable operator procedure.
