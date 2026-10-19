# LQI validation, controlled testing and rollback

Date: 2026-10-18. **Engineering result: reproduced zero originates in the SDK APS field and is preserved correctly by the bridge/host. Underlying RF-versus-SDK semantics remain UNDETERMINED; no corrective measurement patch is justified.** Authorized near/bathroom/return-near diagnostics are complete and the original application is restored. A separate checksum-boundary source correction passes automated tests and C6/H2 builds; corrected C6 hardware deployment, live boundary test and six-event sensor smoke test subsequently passed under fresh approval. This is not a completed production release.

Related: [investigation and hypothesis matrix](LQI_RSSI_Investigation.md), [wire audit](ZNP_AF_INCOMING_MSG_Audit.md).

## Original versus proposed source

| Aspect | Original working implementation | Proposed source, diagnostics OFF | Temporary diagnostics ON |
|---|---|---|---|
| LQI conversion | Saturate SDK int to 0..255 | Same policy through tested constexpr helper | Same policy; raw int also retained |
| Measured zero | Passed as 0 | Passed as 0 | Passed as 0; no minimum/smoothing/history |
| Negative integer | Saturated to 0, provenance lost | Same | Same wire byte; raw signed value/range flag retained |
| RSSI / SDK validity | Not exposed in APS indication | Unchanged | Reported unavailable, never synthesized |
| AF layout/payload/FCS | Existing 4481 layout | Unchanged | Unchanged; records encoded LQI and bridge sequence |
| Diagnostics | Existing F0 radio error query | F0 unchanged; F1 absent | F1 is a new read-only experimental diagnostic query |
| Callback / dispatch | Existing APS handler and return semantics | Unchanged | Same handler; bounded RAM observation only |
| Network and persistence | Existing saved network | No configuration/layout/security changes | Same; no new persistent records or NV writes |

There is no proposed measurement correction. Extracting the expression and adding optional diagnostics make it testable/observable. Raw values outside 0..255 are not automatically declared SDK sentinels; the matching SDK documents no such sentinel.

## Changed files

- `EspRadio.cpp`: use `lqiToByte` with identical behavior; optional capture and queue outcome in the existing callback.
- `LqiMetadata.h` (new): tested original saturation and explicit range check.
- `LqiDiagnostics.h/.cpp` (new): 64-record RAM ring, lock-protected capture/read, no application bytes, keys, logging or storage writes.
- `RadioBackend.h`: diagnostic ID adds four bytes to ApsRx only when enabled.
- `ZnpBridge.cpp`: optional encoded-byte/sequence correlation and read-only UTIL F1; existing command replies unchanged when disabled.
- Tests: `test_lqi.cpp`, `test_lqi_diagnostics.cpp`, `lqi_host.cjs`, synthetic fixtures, `run_lqi.py`; native runner includes the standard LQI regression.
- Tools: `znp_lqi_snapshot.py` and `lqi_mqtt_capture.py`; Python tests cover decoding, zero preservation, unavailable fields and statistics.

No firmware release/compatibility version increment was made. No installed host package, startup/shutdown script, commissioning/routing/security code or persistence format was edited.

## Automated evidence

Checkpoint/log directory: `artifacts/lqi-investigation/20261018T200035Z/` (also named by `CURRENT.txt`).

| Check | Result |
|---|---|
| Pre-edit native protocol 20 / NV 12 / APS ledger | PASS |
| Pre-edit Python tests, 10 | PASS |
| Pre-edit isolated herdsman 11.1.0 reset/resume | PASS; simulated radio only |
| Production AF encoding, 45 LQI/length cases, boundaries/payload/FCS/neighbor fields | PASS |
| Installed herdsman 10.10.0 parser and adapter, 45 frames | PASS |
| Six standard/EXT fixed synthetic fixtures and bad-FCS rejection | PASS; EXT is host-fixture-only |
| Installed Z2M 2.14.2 publication method: zero, boundaries, undefined, later LQI update | PASS; temporary settings, inert MQTT sink |
| TS0203 IAS converter does not synthesize LQI | PASS |
| Ring overwrite/bounds, raw -1 preserved, encoded-byte correlation, read-only query | PASS |
| Final native protocol/NV/APS/LQI and isolated startup/resume | PASS |
| Python collector/parser/bounded-retry tests, 17 total | PASS |
| C6 baseline / diagnostics OFF / diagnostics ON compile/link | PASS, pinned original environment |
| Historical raw ZNP -> parsed host, 10 pairs | PASS, all match, no zero raw sample |
| Historical host latest -> MQTT, 7 pairs | PASS, all match, no zero raw sample |
| Same-room operator test | 10 cycles at 3 ft reported; white activity/green idle confirmed. Exact physical delivery rate INCONCLUSIVE: 22 publications versus 20 counted transitions |
| Near raw SDK -> bridge -> encoded byte -> ordered MQTT LQI | PASS, all 22 equal; 81..209, no zero/negative, no queue drops/overwrites/missing records |
| Bathroom raw SDK -> bridge -> encoded byte -> ordered MQTT LQI | PASS, all 21 equal; histogram 0:11, 5:4, 10:2, 15:4; no negatives/drops/overwrites/missing records |
| Bathroom physical delivery reconciliation | 10 cycles reported; 21 IAS indications/publications, 19 observed state changes after first; exact loss/delay/duplicate rate INCONCLUSIVE |
| Return-near metadata | PASS: 20 raw/bridge/encoded/MQTT values agree; 173..209, no zeros/drops/overwrites; operator reported 10 cycles |
| Intermediate/orientation/reference comparison | NOT RUN |
| Application write/readback, protected storage unchanged | PASS; authorized diagnostic application only, full before/after flash comparison |
| Flashed startup / retained paired-device database | PASS; host logs resumed network and existing UZ-6D |
| Real serial diagnostic retrieval | PASS with timeout observation: snapshots 22/43 records succeeded; 63-record retrieval required one recorded retry at ID 45 due to the separate existing FE checksum defect |
| Fresh battery delivery in this diagnostic run | NOT ESTABLISHED; initial cached publication is insufficient |
| New power-cycle or security/NVS restoration test | NOT RUN; outside this investigation's authorized scope |
| Runtime callback latency, heap trend and task stack high-water mark | NOT RUN; no fabricated measurements |

Commands actually run, with output captured in the checkpoint:

```powershell
python tests/run_native.py --compiler C:/Ruby33-x64/msys64/ucrt64/bin/g++.exe --host
python -m unittest discover -s tests -v
python tests/run_lqi.py --compiler C:/Ruby33-x64/msys64/ucrt64/bin/g++.exe --output YOUR_CHECKPOINT
```

Baseline was compiled from the checkpointed source in `build/lqi-baseline-source/`. OFF and ON were built separately, without using the flash/upload action or overwriting `artifacts/esp32c6`:

```powershell
$checkpoint = (Get-Content artifacts/lqi-investigation/CURRENT.txt -Raw).Trim()
$fqbn = 'esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=zigbee_zczr,ZigbeeMode=zczr,DebugLevel=none'
arduino-cli compile --jobs 8 --fqbn $fqbn --build-path build/lqi-off-c6 --output-dir "$checkpoint/build-diagnostics-off" Arduino/TI_ZNP_ESP32_RealBridge
arduino-cli compile --jobs 8 --fqbn $fqbn --build-property 'compiler.cpp.extra_flags=-DZNP_LQI_DIAGNOSTICS=1' --build-path build/lqi-on-c6 --output-dir "$checkpoint/build-diagnostics-on" Arduino/TI_ZNP_ESP32_RealBridge
```

## Measured resource comparison

| Build | Sketch bytes | Static RAM bytes | Receive function bytes / stack frame |
|---|---:|---:|---:|
| Checkpoint source baseline | 727598 | 40492 | 330 / 288 |
| Proposed, default OFF | 727598 | 40492 | 330 / 288 |
| Proposed, ON | 728874 | 42300 | 476 / 352 |
| ON minus baseline | +1276 | +1808 | +146 / +64 |

The baseline and OFF receive-function machine code is byte-identical. OFF ELF contains no diagnostic symbols. ON adds four bytes per queued ApsRx, so the existing 24-element queue requires 96 additional heap bytes for its item storage. No extra diagnostic heap allocation occurs in the callback. The stack figure is the compiled function's prologue allocation, not measured maximum task stack consumption. Runtime timing, contention and event-loss overhead remain unmeasured. `resource-evidence.json` and disassembly of our own receive function preserve the evidence; SDK library binaries were not disassembled.

## Diagnostic interface and limits

Default `ZNP_LQI_DIAGNOSTICS=0`. ON records accepted APS application indications only: SDK raw signed LQI, processing status, source/cluster/profile/endpoints, ASDU length, local receipt uptime, normalized bridge byte, queue outcome and encoded byte/sequence. Rejected status/endpoint/size categories and ZDO metadata are not captured. No payload bytes, keys or credentials are recorded.

The fixed ring keeps the most recent 64 observations. IDs, overwrite and queue-drop counts expose lost history. Lock-protected reads may observe a record before encoding; an unset encoded flag means the zero-initialized output field is **not an encoded measurement**. Range flag means only 0..255, not SDK validity. RSSI and transport-write success are explicitly unavailable. No callback registration/order/return changes and no verbose/unsolicited serial output are added.

Experimental read-only command is UTIL F1, available only when ON:

- Request `00`: response status/schema/capacity/count (one byte each), latest ID, overwritten count and queue-drop count (three LE uint32 values), total 16 bytes.
- Request `01` + LE uint32 ID: status/schema + 28-byte metadata record, total 30 bytes. Status 9 means no retained record; status 2 means malformed request.
- Record layout after status/schema: id/u32, rx_ms/u32, raw_lqi/i32, source/u16, cluster/u16, profile/u16, length/u16, then sourceEP, destinationEP, SDK status, bridge LQI, encoded LQI, encoded sequence, flags and reserved (one byte each). Flags: 1=in byte range, 2=queued, 4=encoded.

The utility only requests F1; it does not send reset, startup, NV, permit-join or AF transmit commands. An OFF build returns the existing unsupported-command error. Do not run it concurrently with Zigbee2MQTT. Native tests and authorized physical retrieval of 22 records both passed.

## Operator-controlled physical protocol

**Approval is required before any live capture, service stop/start, bootloader access or flash.** These are future procedures, not actions performed by this report. Use only the existing paired UZ-6D; do not remove, re-interview or re-pair it. Keep network identity, channel, power, keys and counters unchanged. Stop on loss of normal events or an unexpected network/identity mismatch.

### First, observe the working firmware without flashing

With the operator's approval and the normal host running, a topic-only subscription can collect receipt time, contact and LQI without serial ownership or setting changes:

```powershell
python tools/lqi_mqtt_capture.py --operator-approved-capture --seconds 180 --output artifacts/lqi-tests/A-mqtt.jsonl
python tools/lqi_mqtt_capture.py --analyze artifacts/lqi-tests/A-mqtt.jsonl
```

The tool requires the existing Mosquitto subscriber; it does not install/start services. Retained messages are ignored, but new publications may still contain cached properties. Metadata uses receiver time, not RF time. Statistics keep physical counts, delivery failures, latency and duplicates unknown until reconciled with the operator log. Timeout exit code and capture duration remain visible.

For each condition, capture an explicit starting closed state before counting, then **20 deliberate transitions** (ten open/close cycles), roughly five seconds apart. Record operator time, expected state, observed MQTT state/time, location, separation, board orientation, intervening walls, USB position and relevant environmental changes. Do not count battery/cache updates as duplicate door events. If matching is ambiguous, mark it ambiguous rather than assigning packet loss.

| Test | Conditions | Evidence / acceptance |
|---|---|---|
| A | Fixed same-room separation, about 2-3 m; no direct contact | All 20 counted transitions reconcile; timestamp/LQI distribution and zero frequency recorded |
| B | Stable intermediate location with recorded walls | Same counts and observations; compare with A without assuming an LQI/distance formula |
| C | Original bathroom location, configuration/orientation recorded | Same; determine whether zero reports recur while event delivery remains intact |
| D | Fixed location; repeat with documented board rotations, keeping other conditions fixed | Record changes as measured comparisons, not proof of one mechanism |
| E | Existing genuine TI reference evidence only, if useful | No sensor migration, conflicting coordinator, network restoration or re-pairing. New reference-network work needs separate authorization |

Record missing/delayed/extra transitions against operator intent. An unobserved event can originate in the sensor, RF, bridge or host; it is not automatically RF packet loss. LQI distributions from MQTT describe publications. Raw diagnostic distributions describe the captured APS indications. No RSSI is available from this instrumentation.

### If raw diagnostics are authorized

Before deployment, verify ownership of the Windows USB device, obtain fresh host-data copies after an approved clean shutdown, and confirm hashes and unchanged partition/network configuration. This session subsequently performed an authorized full-flash **read**, preserving a private raw image. It must never be blindly restored with old security counters and does not establish a supported TI coordinator-state restoration procedure.

Deploy only the compiled ON **application** at `0x10000`, using the existing 4 MB/native-USB settings. Do not erase flash/NVS, upload the merged image, alter bootloader/partitions or change the host configuration. The operator must approve the exact service interruption and app-only write before it is executed. Announce the flash first. The production host should then resume its existing network, with fresh contact and battery reports verified before any location test.

Repeat A-C in short bursts. After each burst, an approved host shutdown releases COM17 while the powered radio retains its RAM ring. Immediately retrieve it, without resetting the board:

```powershell
# Only after explicit approval and Stop-Lab.cmd has released COM17:
python tools/znp_lqi_snapshot.py --operator-approved-port-access --port COM17 --output artifacts/lqi-tests/A-sdk.json
# Restart the normal host only within the approved test procedure.
```

Read the summary/overwrite counts and correlate raw integer, in-range flag, bridge byte, encoded byte/sequence, cluster and time. Use the host's existing detailed trace if available. Enabling debug logging or a serial transport tap is a separate approved change; the current information-level log cannot prove per-packet zero wire correlation. No package patches are needed for the prepared tests. If the ring overwrites the interval or the capture lacks an encoded flag, shorten the burst/repeat rather than invent missing metadata.

Interpretation: raw negative plus bridge 0 establishes clamping occurred, but the SDK meaning still needs confirmation. Raw 0 plus encoded 0 excludes a conversion change for that packet, but not SDK-default versus real zero. Nonzero raw/bridge with zero encoded would locate a bridge defect; nonzero encoded with zero latest host value requires exact transport/host correlation. Never replace zero based on appearance alone.

## Authorized hardware phase — in progress

Operator approval covered the temporary application-only diagnostic flash, Windows lab stop/start and existing paired sensor test. Evidence directory: `artifacts/lqi-investigation/20261018T200035Z/hardware-20261018T201612Z/`.

- Cleanly stopped the managed lab and copied five host data/configuration files before touching COM17.
- Read the complete 4 MB flash privately. Its app0 application exactly matches the known-good checkpoint image; app1 begins erased.
- Flashed only the diagnostic application at `0x10000`; esptool hash verification passed. Full readback matches candidate SHA-256 `7a03fa9b9f5e1df100c7df3ab782378e8fa5cdf0224dff8c29d59ede06c4fc45`.
- Before restarting the host, comparison confirmed every checked non-app0 region unchanged, including NVS/OTA and Zigbee storage/factory. See `post-flash-storage-verification.json`.
- At 16:19:24 local time, unchanged Zigbee2MQTT reported `zigbee-herdsman started (resumed)` and listed the existing UZ-6D; frontend and MQTT came online. The initial cached contact/battery publication is not counted as fresh RF evidence.
- Started a metadata-only, non-retained MQTT capture. Operator reported 10 cycles at 3 ft and correct white/green LEDs. The fixed near window holds 22 publications and 22 accepted IAS records, all matching raw/bridge/encoded/ordered MQTT LQI (81..209). No drops, overwrites, missing records, negatives or zeros. SDK-to-MQTT time-offset spread is about 4.75 ms; absolute latency is not measured. Exact physical event reconciliation is inconclusive because counted-action timestamps/boundaries were not recorded.
- Read the RAM records during an approved clean host interruption, without resetting the board, then restarted the unchanged lab successfully.
- Bathroom test reproduced 11 zero publications in 21 new IAS indications. Raw SDK values were already zero for all 11; all 21 values matched bridge/encoded/ordered MQTT. No drops/overwrites/missing records. The second snapshot retained 43 total records; bathroom filtering is explicitly IDs 23..43 after the near phase. Both phases used the same firmware and board position. Return-near captured 20 matching IAS/MQTT records, 173..209, no zeros.

These observations do not establish the cause of zero. No measurement correction, re-pairing, reset-to-factory, configuration change or old-counter restoration has been performed.

## Rollback and release status

The original application was restored and independently verified, with all non-app0 bytes unchanged across rollback. Production version remains unchanged; this investigation is not a validated firmware release. Preserve the original application for application-only rollback after diagnostics, retaining the current network storage and counters.

If a later authorized diagnostic deployment needs rollback:

1. Preserve its diagnostic results and obtain approval to stop the host and write the application. Stop cleanly; confirm exclusive COM17 ownership.
2. Use the checkpoint's **`firmware-before/TI_ZNP_ESP32_RealBridge.ino.bin`**, SHA-256 `2af3bfc84eb28258ae2d191188929a99f88eb30c6edb0545997f87fb2e6a2aeb`. Verify its hash before writing. The authorized pre-test flash readback independently confirmed that this image matches the previously installed application.
3. Write only that application at `0x10000` with the existing settings. **Do not erase or restore NVS, partitions, bootloader, merged/full-flash images or old security counters.**
4. Restart the unchanged host against the retained network. Verify fresh open/closed reports, battery handling, normal MQTT fields and green/white LED behavior without re-pairing.
5. If network state or host data has changed, firmware rollback alone cannot undo it. Stop and assess the separate SDK/NVS and host-data recovery needs; the live-copy checkpoint and incomplete TI backup do not establish safe automatic network restoration.

Returning to default-OFF source can also be built for a future reviewed update, but it is not necessary to change the current device. Keep the baseline and candidate images separately named; do not overwrite the known-good artifact.

## Final engineering decision

1. **Root cause:** zero provenance confirmed at SDK APS; underlying genuine-RF versus SDK metadata semantics undetermined. No LQI encoder/host value defect demonstrated; the separate decoder defect is corrected and reported independently.
2. **Evidence:** All 63 near/bathroom/return-near raw SDK/bridge/encoded/MQTT values agree; all 11 bathroom zeros were raw 0. Example ID 24: status 0, IAS cluster 0x0500, raw/bridge/encoded/MQTT all 0. Source trace and independent wire audit are linked above.
3. **Changes:** behavior-neutral conversion helper, opt-in RAM diagnostics, tests/tools/docs; separately justified `ZnpFrame.cpp` valid-FE resynchronization correction. No cosmetic LQI replacement.
4. **Tests:** automated checks/builds, app write/readback, resumed startup and two-location metadata correlation PASS. Exact physical delivery rate inconclusive; return-near PASS; broader tests NOT RUN.
5. **Regression:** OFF receive machine code/resources match baseline; diagnostic flash preserved all checked non-app0 storage, host resumed the existing network, F1 retrieval passed and events continue. Runtime overhead/stress acceptance remains incomplete.
6. **Resources:** original diagnostic ON +1276 flash, +1808 static RAM, +96 queue bytes, +64-byte callback stack frame. Final corrected OFF C6 is +4 sketch bytes versus baseline and unchanged static RAM (40492); runtime high-water/latency unmeasured.
7. **Deployment:** no corrective update justified. The temporary diagnostic deployment is complete and rolled back. A separate corrected-framing default-OFF build was subsequently authorized, deployed app-only and validated on C6. The host is running with retained pairing/network state.
8. **Uncertainty:** raw zero is confirmed, excluding a negative clamp for these samples. Genuine zero versus SDK-internal default, packet-matched RSSI, broader categories and isolated environmental mechanisms remain unresolved.
9. **Recommendation:** preserve measured zero and the retained network; change production measurement behavior only after a cause is demonstrated. Keep the previously working image for application-only rollback.

## Subsequent checksum correction

The separate [checksum-boundary report](ZNP_Checksum_Boundary.md) records the original failing reproducer, minimal production decoder patch, all 256 valid-FCS regressions, preserved corrupt/timeout recovery, native/host/Python checks, C6/H2 builds, and the operator-authorized C6 hardware result. The live valid-FE frame plus immediate PING passed without retry. Full readbacks preserved all bytes outside app0. The existing paired sensor then delivered all six expected alternating MQTT states from three cycles. The corrected default-OFF application is now deployed; unchanged Windows services/process arrangement and network configuration remain in use. No claim is made for new battery RF delivery, H2 RF acceptance, long-duration reliability or calibrated signal measurements.
