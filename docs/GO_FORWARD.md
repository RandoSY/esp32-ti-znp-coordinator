# Go-forward plan: reliability and compatibility acceptance

Updated 2026-10-18. These are planned tests and engineering tasks; listing them here does not mean they passed.

Current high-priority investigation: [LQI/RSSI correctness](LQI_RSSI_Investigation.md), with [protocol audit](ZNP_AF_INCOMING_MSG_Audit.md) and [validation/controlled testing/rollback](LQI_Validation_Report.md). Zero LQI has not been proven erroneous. Keep the working firmware; any live test, service interruption or diagnostic flash requires explicit operator authorization under this investigation's safety constraints.

The authorized same-room/bathroom diagnostic comparison located the reproduced zeros in the **Espressif APS indication itself**: all 11 bathroom zeros were raw 0, preserved through bridge/ZNP/MQTT. Near values were 81..209; bathroom values 0..15. No clamp, encoder or host replacement defect was demonstrated. Preserve these measurements. Further causal work should obtain vendor clarification or safely correlate packet-matched lower-layer metadata with APS, under separate review; reading a global "most recent RSSI" does not establish that it belongs to the application packet. Do not install a competing receive hook or upgrade the working stack merely to obtain a more attractive number.

Future reliability trials need timestamped operator actions and explicit starting/ending states, alongside the metadata capture, to measure physical delivery loss/delay/duplicates. Current publication counts alone cannot certify those rates or full TI Zigbee 3.0 equivalence. Retain the existing broader acceptance plan and use the existing paired sensor without recommissioning.

The separate [valid-FE checksum correction](ZNP_Checksum_Boundary.md) is now tested in native regressions, C6/H2 compilation and an explicitly authorized C6 app-only deployment. The live boundary test and six reports from three paired-sensor cycles passed. The corrected application runs with diagnostics OFF; all non-app0 bytes were preserved. Continue broader reliability and device-family acceptance without treating this smoke test as a complete release qualification.

## Established baseline

**End-to-end functionality passed for the UZ-6D.** The WeAct ESP32-C6 receives real Zigbee reports, exposes them through its TI ZNP serial personality, and works with unmodified Zigbee2MQTT 2.14.2 / herdsman 10.10.0 and Mosquitto 2.1.2 on Windows COM17. Contact open/closed transitions and battery reports were received. The operator confirmed green at idle and white receive flashes. The supplied frontend screenshot also shows repeated contact transitions.

This establishes the tested sensor path. It does not establish complete CC2652P behavior, all-device compatibility, or complete TI security backup/restore. Existing evidence is in [RF acceptance](RF_ACCEPTANCE.md); implementation limits are in [the compatibility matrix](COMPATIBILITY_MATRIX.md).

## Execute in this order

The counts below are initial lab acceptance targets, not TI or Zigbee certification requirements. Record each attempt and any failures; do not count a successful retry as a clean first pass.

### 1. Restart and power-cycle recovery — next priority

- Restart the Windows application with `Stop-Lab.cmd` / `Start-Lab.cmd` ten times.
- In five additional cycles, stop the application, disconnect/reconnect board power, confirm the Windows port, then start the application. Do not erase or re-pair.
- Separately exercise an unexpected board disconnect while the application runs. Record its error and whether reconnect plus a normal application restart recovers. Keep deliberate power-loss tests on the isolated lab network.
- After every recovery, physically open and close the sensor and verify new, timestamped MQTT reports for both states. Check the saved network identity and sensor identity, and observe green idle / white reception.

**Pass:** the same sensor resumes without pairing or changed network settings, fresh reports reach MQTT, and no unexplained storage fault or persistent radio error occurs. A cached state displayed immediately after startup is not a fresh RF report. Record recovery time. Automatic reconnect behavior is a separate result from recovery after a manual host restart.

### 2. Broaden device and routing coverage

Add another battery sensor, a mains-powered Zigbee router, and a controllable light or plug. Use different device families where available. Record model, manufacturer, firmware and endpoints.

Verify joining, complete interview, configuration and reporting. For the actuator, send repeated on/off commands and check its physical state plus device responses/reports. Confirm router attachment/path through topology or management evidence before claiming routed traffic; merely adding a router is insufficient. Then exercise the sensor through that route and check recovery when the router is restored after a disconnect.

**Pass:** each device completes its applicable operations; commanded states and returned states agree, routed traffic is demonstrated, and other joined devices continue operating. Record failures by model and operation rather than treating one device as representative of all Zigbee devices.

### 3. Join, leave and rejoin lifecycle

On a lab device, repeat normal host removal and joining at least three times. Where supported, distinguish a confirmed leave from a forced host-database removal. Check successful re-interview, IAS enrollment/reporting for the contact sensor, updated short addresses, and absence of duplicate/stale device records. Confirm joining closes when the window expires and the LED returns to green.

**Pass:** the device returns to useful operation on every cycle, without coordinator erase or unrelated devices needing to pair again. Record unsupported device behavior separately from bridge failures.

### 4. Sustained operation and concurrent traffic

Run an overnight test of at least 12 hours with multiple devices. Schedule a known number of actuator commands and controlled sensor operations at recorded intervals. Include a bounded burst of distinct requests to exercise concurrent confirmations. Keep default logging manageable; capture detailed traces around failures.

Track expected versus received operations, non-success/missing confirmations, unexpected resets, stuck requests and queue overflows. Record heap/queue counters if instrumentation exposes them; otherwise mark those measurements unavailable rather than assuming no memory loss. Recheck ordinary commands and contact reporting at the end.

**Pass:** no unexplained hangs, resets, mismatched confirmations or dropped controlled operations; all devices remain usable. Investigate any failure and repeat the affected test after correction. Battery devices' unscheduled periodic messages are not a reliable denominator for packet-loss measurements.

### 5. Ubuntu 22.04 / VirtualBox acceptance

The Ubuntu installation is handled separately. This step records its hardware/application acceptance; it does not authorize changing that installation.

Stop the Windows lab before transferring USB ownership. Repeat the serial probe, full Zigbee2MQTT startup and fresh contact-report test using the guest's actual tty path. Exercise USB release/capture and reconnect, recording whether host restart is needed. Keep the guest host's network parameters aligned with the board's existing network.

**Pass:** the guest receives fresh RF contact transitions through MQTT, releases the port cleanly, and the board can return to Windows operation without re-pairing. Record guest host versions, VirtualBox version, USB controller and serial path independently of the Windows result.

## Engineering gate: complete TI security backup/restore

**Known implementation gap, not just an unrun test:** complete per-device address-manager, trust-center/link-key and security-manager state is not exported/restored between virtual TI NV and the actual SDK tables. A generated network-only backup must not be accepted as a complete migration image.

Implement the missing mappings and verify keys, frame counters, network identity and device state against actual SDK behavior. Only then test restoration on an isolated disposable network, including recovery onto an erased/spare coordinator and fresh communication without device re-pairing. Keep the original state recoverable; do not run two coordinators with the restored identity/network simultaneously.

**Pass:** restoration preserves the required security state and devices resume bidirectional traffic without security-counter regression. Until that is demonstrated, describe backup/restore as incomplete.

## Further compatibility gates

- Compare command/reply/event traces with a real CC2652P using the same host version and equivalent lab scenarios. Separate formatting differences from operational differences.
- Exercise channel changes, key rotation and interrupted configuration on disposable networks after the relevant mappings are implemented.
- Test physical H2 hardware and UART transport independently; a successful build is not hardware acceptance.
- Validate additional endpoint profiles, groups/bindings, management operations and unsupported-command behavior before broadening compatibility claims.

## Record results consistently

For each test, record:

| Field | Required evidence |
|---|---|
| Test and result | ID, date, pass/fail/not run, attempt count and failure details |
| Firmware | Source revision or source snapshot, binary hash, core version, board and transport |
| Host | OS, Zigbee2MQTT/herdsman/Mosquitto versions; VM details when applicable |
| Devices | Model, firmware, device identity and relevant endpoints |
| Procedure | Exact actions, expected operations, observed results and recovery time |
| Traffic | Timestamped MQTT/application/serial evidence appropriate to the test |
| Limits | Missing instrumentation, manual recovery, unsupported paths and unresolved failures |

Store private captures under ignored `artifacts/` or `lab-zigbee2mqtt/`. Review/redact keys and credentials before sharing evidence. Add verified outcomes to [RF acceptance](RF_ACCEPTANCE.md) and update [the compatibility matrix](COMPATIBILITY_MATRIX.md) only after the corresponding test passes.

Use [Getting Started](START_HERE.md) for Python commands and RGB meanings, and [installed software](INSTALLED_SOFTWARE.md) for Windows startup, shutdown and VM handoff.
