# LQI/RSSI investigation

Date: 2026-10-18. **Zero provenance CONFIRMED at the Espressif APS indication; underlying RF-versus-SDK cause UNDETERMINED. No LQI correction is justified.**

The reported symptom is real: existing MQTT logs contain zero-valued `linkquality` while contact states continue changing. A zero publication alone does not distinguish a genuine measurement, missing SDK metadata, a clamped negative value, or publication of the host's latest stored value. No arbitrary minimum, RSSI-derived replacement, smoothing or historical-value substitution has been introduced.

## Protection and checkpoint

At the start of the investigation, this workspace was not a Git repository, so there was no project commit or working-tree status to record. A repository was subsequently initialized for the explicitly requested public publication; the pre-edit ZIP remains the original checkpoint. Before edits, 58 source/documentation/tool files were preserved in a ZIP with SHA-256 manifest. Existing firmware artifacts, build settings and five accessible private host-data files were also copied. All five files were unchanged during their individual copies; the running application was not frozen, so cross-file transactional consistency is not guaranteed.

Checkpoint: `artifacts/lqi-investigation/20261018T200035Z/`; `CURRENT.txt` in its parent identifies it. It includes `source-before.zip`, `checkpoint.json`, `environment.json`, `INITIAL_READ_ONLY_FINDINGS.md`, `firmware-before/`, and `private-host-backup/`. Private files remain ignored and must not be published unreviewed.

The initial checkpointed application was the on-disk last programmed artifact. During the subsequently authorized hardware phase, a complete private 4 MB flash read confirmed that the installed application exactly matched it. This raw snapshot contains sensitive coordinator state and must remain private; it must not be blindly restored with old security counters. The existing host `coordinator_backup.json` is available, but per-device TI security-table export/restore is incomplete.

The initial investigation and automated tests were isolated from the live device. The operator then explicitly authorized a temporary application-only diagnostic flash, Windows lab stop/start, and an existing-sensor test. On 2026-10-18 at approximately 16:18 local time, the diagnostic application was written at `0x10000`. Full before/after readbacks verify the candidate and byte-identical NVS, OTA data, Zigbee storage/factory, bootloader, partition table and remaining non-app0 regions. Zigbee2MQTT subsequently logged `started (resumed)` and retained the UZ-6D. Physical captures are complete; final results and restoration are recorded below. Network configuration, membership, installed packages and dependencies were not deliberately changed. Normal resumed-stack persistence and security counter progression must not be undone.

## Exact environment

| Component | Inspected version/evidence |
|---|---|
| Hardware / transport | WeAct ESP32-C6FH4, 4 MB, native USB CDC; recorded Windows COM17 |
| Emulated identity | Product 1, version 2.7.1, revision 20240710; unchanged |
| Arduino core | ESP32 3.3.11, installed board package and build metadata |
| ESP-IDF | 5.5.5, commit `b774170ff46`; installed `versions.txt` and `esp_idf_version.h` |
| ESP Zigbee library | `espressif__esp-zigbee-lib` 1.6.8; `versions.txt` and `esp_zigbee_version.h` agree |
| ZBOSS library | `espressif__esp-zboss-lib` 1.6.4; installed package metadata |
| Arduino CLI / Node | 1.4.1 / 22.20.0 |
| Actual full host | Zigbee2MQTT 2.14.2, bundled herdsman 10.10.0, converters 26.115.1, MQTT client 5.16.0 |
| MQTT broker | Previously installed Mosquitto 2.1.2; restarted within the authorized test, not upgraded |
| Separate simulated-radio test dependency | herdsman 11.1.0; not the full application's bundled version |
| Installed Z2M Git revision | `5c0c1c60078e1737c5dee5f8217583bca7610e66`, clean status |

Installed SDK root: `C:/Users/rasyoung/AppData/Local/Arduino15/packages/esp32/tools/esp32c6-libs/3.3.11`. Host paths below are relative to `build/zigbee2mqtt/`. Source lines refer to the investigated source after adding diagnostics.

## Receive-path traceability

| Stage / source | Type, availability and transformations | Zero, lifetime and limitations |
|---|---|---|
| 802.15.4 receive metadata: installed `include/ieee802154/include/esp_ieee802154_types.h:78-86` | Lower-level `esp_ieee802154_frame_info_t` includes `uint8_t lqi`, `int8_t rssi`, channel and hardware timestamp | Not directly supplied to our APS handler. No extra driver callback was installed; its ordering/ownership must not be disturbed |
| Espressif/ZBOSS MAC/NWK -> APS | Library implementation supplies NLDE LQI to APS | Internal propagation cannot be proven from public headers. No binary-library disassembly was used. Missing/default behavior remains unknown |
| SDK `aps/esp_zigbee_aps.h:93-109` | `esp_zb_apsde_data_ind_t`: `int lqi`, `uint8_t status`, addresses/endpoints, profile, cluster, ASDU pointer/length, security status, reserved `int rx_time` | No RSSI member, measurement-validity flag or documented LQI sentinel. The C type can represent negatives; this does not prove the SDK emits them. Zero is representable and must be preserved |
| Registration: `EspRadio.cpp:83`; callback `EspRadio::indication`, line 205 | Same existing callback receives struct by value. Nonzero processing status, null ASDU, payload >220 or destination endpoint 0 is rejected | Rejected indications return false for normal stack processing. Accepted application frames return true as before, leaving host ownership of ZCL. No second callback/hook added |
| Conversion: `EspRadio.cpp:207`, `LqiMetadata.h` | Existing clamp: raw <0 ->0; raw >255 ->255; otherwise exact `uint8_t` value | Real 0 passes unchanged. Undocumented negative values would become 0; positive overflow would become 255. No evidence yet that either occurs on hardware |
| Queue: `ApsRx` in `RadioBackend.h`; `xQueueSend` in `EspRadio::indication`; `popRx` in `EspRadio.h` | LQI is a stored `uint8_t`; payload is copied into the fixed array, then the whole record is copied into a 24-element FreeRTOS queue | No retained SDK ASDU pointer or reference. Record initialized to zero before assignments; LQI explicitly assigned. Full queue drops the message and increments the existing drop count; it does not generate a zero-LQI replacement |
| Bridge: `ZnpBridge::poll`, line 203; `emitIncoming`, line 184 | Copies queued LQI to payload byte 9, alongside independently assigned fields | No scaling, fallback or history. Header and payload are serialized synchronously from local storage |
| Framing: `ZnpFrame.cpp::write`; sketch `serialWriter` | `FE`, length, command `44 81`, payload, XOR FCS; native USB 115200 configuration unchanged | Writer return byte count is not captured. Diagnostics mark transport status unobserved rather than fabricating success |
| Herdsman 10.10.0: `znp/definition.js:978`, `ZpiObject.fromUnpiFrame:59`, `buffalo.js:34` | Standard `incomingMsg.linkquality` is `UINT8`, parsed using `Buffer.readUInt8` | Host does not reinterpret this field as signed RSSI. Checksum is validated by UNPI framing |
| Adapter: `zStackAdapter.js::onZnpRecieved:569`, lines 692/714 | Emits `zclPayload.linkquality = object.payload.linkquality` | Direct numeric passthrough, including 0; extended incoming parsing uses its own layout |
| Herdsman controller: `controller.js::onZclPayload:691`, line 777; `model/device.js:204` | Assigns `device.linkquality = payload.linkquality`; setter stores it unchanged | Latest received application-message value, not a per-contact immutable snapshot |
| Device converters: `fromZigbee.js:1098/1113`, `devices/tuya.js:2862` | TS0203 IAS converters derive contact/tamper/battery-low; battery converter handles battery attributes | IAS converters do not derive LQI or substitute zero. Different message types can publish at different times |
| Z2M `lib/extension/receive.ts` -> `lib/controller.ts:383-456` | Async conversion/debounce/throttle can delay publishing. At lines 436-438 the publisher takes current `entity.zh.linkquality` if it is not undefined | A later packet can update LQI before an earlier contact payload publishes. Existing values can also accompany cached-state publications. An MQTT publication is not necessarily one newly measured RF packet |
| MQTT / state cache: `lib/state.ts:139`, `publishEntityState` | JSON publication and state merging | No LQI rescaling found. Neighbor-table LQI used by `networkMap.ts:317` is a separate management observation, not this byte |

The current firmware emits standard AF incoming messages only. `AF_DATA_REQUEST_EXT` support does not imply `AF_INCOMING_MSG_EXT` emission. ZDO responses travel through separate SDK callbacks and `RadioEvent`, including management neighbor-table entries; these must not be substituted for per-message LQI. IAS commands and battery attribute reports both use the same accepted APS application path.

## Existing measured observations

Six existing application logs were examined read-only. They contain **57 sensor-topic publications**, 14 with LQI zero and two without LQI. The observed numeric range is 0..239. Zero publications occur in `lab-zigbee2mqtt/log/2026-10-18.11-49-17/log.log`, lines 19 and 21-33, between 11:50:38 and 11:53:09 local. They include contact true and false (8 and 6 publications respectively).

These are **publication counts**, not independent SDK measurement or physical transmission counts. Operator event times/counts, room locations and orientations were not recorded with those logs. Thus no packet loss, duplicate rate or RF calibration can be inferred.

Ten earlier AF debug records are available: six cluster 0x0500 and four cluster 0x0001, LQI 25..96. All ten reconstructed raw ZNP frames have valid FCS and match the host's parsed LQI/source/sequence. All seven corresponding MQTT publications match the latest preceding host LQI. None of these raw captures covers the later zero interval. No raw SDK integer or RSSI is present in existing captures.

Evidence: `existing-host-observations.json` and `wire-host-correlations.json` in the checkpoint contain metadata only. Application bytes, keys and credentials were not copied into these observation reports.

## Hypothesis assessment

Confidence refers to the stated evidence, not an assumed probability of the cause.

| Hypothesis | Supporting evidence | Contradicting evidence / limit | Safe discriminating test and result | Assessment |
|---|---|---|---|---|
| H1: genuine weak RF | Same firmware/board: near raw 81..209 versus bathroom 0..15, with 11 raw zeros | No simultaneous packet-matched RSSI or controlled attenuation; location changes also change orientation/body/interference | Near/bathroom/return-near measured; return range 173..209 with no zeros | Environmental RF effect supported, moderate confidence; exact zero semantics unproven |
| H2: missing SDK metadata | APS API has no validity flag/sentinel documentation; 11 zeros originate in SDK field | Varying values within the same IAS path and location-dependent distribution argue against a blanket missing-metadata default | Raw SDK capture obtained for zero reports; SDK-internal validity remains unavailable | Unresolved: cannot certify genuine versus missing SDK zero |
| H3: incorrect conversion | An actual negative SDK integer would be clamped to 0; type permits it | All 43 near/bathroom raw values are in range and equal bridge/encoded bytes; each observed zero was raw 0, not negative | Boundary tests and signed hardware captures PASS | Excluded for reproduced zeros; clamp remains unchanged |
| H4: serialization defect | A shifted unsigned byte could produce misleading values in principle | Correct offset/width/FCS; 45 production frame cases, installed parser tests and 10 historic wire matches | Golden standard/EXT host fixtures and neighboring-field tests PASS | Strong evidence against defect in tested standard path |
| H5: stale/overwritten firmware metadata | Concurrent tasks and bounded queues require care | Struct/payload copies, no retained callback pointer; record isolation tests PASS; all 43 raw/queued/encoded values agree, no drops/overwrites | Near/bathroom capture PASS; stress/load testing NOT RUN | No defect found in reproduced path; SDK internals/load remain unmeasured |
| H6: host interpretation | Publisher uses latest device LQI after asynchronous conversion; cache behavior can decouple payload from measurement | All 43 SDK values match MQTT in order, including all 11 bathroom zeros | Offline publisher and live correlation PASS | Host-created zero excluded for reproduced samples; general timing limitation remains |
| H7: packet category | ZDO and ZCL use distinct paths; battery/default responses can arrive separately | All 43 live diagnostic records are the same IAS cluster/status/endpoint path, including zero and nonzero | IAS comparison PASS; new raw battery/ZDO category comparison NOT RUN | Category difference unnecessary to explain reproduced IAS zeros; wider behavior untested |
| H8: environment/coexistence/orientation | Fixed board/software, changed sensor location produces markedly different values | No controlled interference/orientation/USB experiment; no packet-matched RSSI; sketch does not configure Wi-Fi | Near/bathroom measured; isolated orientation/coexistence tests NOT RUN | Environmental effect supported; individual mechanism undetermined |

LQI's conventional range includes zero and is not a calibrated distance or linear dBm scale. Continued reception at zero is not by itself proof of a bug. The matching APS API does not document a range/sentinel contract beyond identifying the supplied NLDE LQI; range checking cannot prove measurement validity.

## Changes and next decision

### Authorized near-location measurement

The operator reported ten open/close cycles at **3 feet (approximately 0.91 m)**, rather than the requested 2-3 m, with white activation flashes and green afterward. Before any board reset, read-only F1 retrieval returned 22 accepted IAS (`cluster 0x0500`, status 0, source `0x3FB6`) records: no missing IDs, ring overwrites or queue drops. All were queued and encoded. Raw SDK LQI ranged **81..209**, with no negatives or zeros. Every raw value equalled its bridge and encoded value; all 22 values also matched the 22 MQTT publications in order. The spread of MQTT-receipt minus SDK-relative-time offsets was approximately 4.75 ms, supporting the ordered correlation; it is not a calibrated end-to-end latency measurement. No separate live serial wire tap was used.

The captured window contained 21 state changes after its first published state, versus 20 operator-counted physical transitions. Exact test boundaries/action timestamps were not logged, so this result cannot certify a physical loss/duplicate rate or assign the additional activity. Near-test evidence is in `A-near-sdk.json`, `A-near-completed-mqtt.jsonl` and `A-near-analysis.json` within the private hardware directory. The ongoing broad recorder file is not the fixed near-test evidence boundary. No zero was reproduced near the coordinator.

### Authorized bathroom measurement

The operator confirmed ten cycles in the original bathroom. Approximate distance, wall count and LED observation were not supplied for this run. The ring retained the earlier records across the normal host stop/start, so phase selection explicitly uses **IDs 23..43**, after the near snapshot's last ID 22. Do not count all 43 retained records as bathroom traffic.

All **21 new IAS indications** have status 0, source `0x3FB6`, profile `0x0104`, endpoints 1->1 and payload length 9. Raw LQI histogram is **0:11, 5:4, 10:2, 15:4**. Every raw integer equals its bridge byte and encoded ZNP byte; the complete sequence equals MQTT. No negative/out-of-range input, queue drop, missing record or overwrite occurred. SDK/MQTT relative-time offset spread is approximately 6.08 ms, supporting correspondence without establishing absolute latency. There are 19 contact changes after the first state and one same-state publication; exact physical timestamps are absent, so the additional IAS indication cannot be assigned to a retry, duplicate or extra action.

For a concrete reproduced zero, `C-bathroom-sdk.json` record **ID 24** has `rx_ms=259382`, `sdk_status=0`, `cluster=1280`, `sdk_lqi_raw=0`, `bridge_lqi=0`, `znp_lqi_byte=0`, `znp_sequence=24`. The corresponding ordered MQTT publication is also 0. Ten other records show the same raw-to-publication zero preservation. Evidence: `C-bathroom-sdk.json`, `C-bathroom-completed-mqtt.jsonl`, `C-bathroom-analysis.json`.

This confirms that the reproduced zeros already exist in the SDK indication. Our clamp does not create them, and the tested encoder/host path preserves them correctly. Location-dependent values support an RF/environment explanation, but cannot distinguish genuine low-quality SDK measurements from an internal SDK default or metadata limitation. No RSSI or validity flag was manufactured. Return-near results are recorded below.

No LQI correction is justified yet. The existing saturation expression was extracted into a tested constexpr helper without changing its results. The baseline and diagnostics-off compiled receive functions are byte-identical. Optional `ZNP_LQI_DIAGNOSTICS=1` captures raw integer, source/cluster/profile/endpoints, queue outcome and encoded byte/sequence in a bounded RAM ring. It adds no RSSI, payload logging, extra APS callback, unsolicited serial output or persistent writes. Default is 0.

Read-only diagnostic retrieval is compiled only when enabled. The diagnostic candidate was subsequently removed after testing; the collector and controlled test/rollback procedure are provided in [LQI validation](LQI_Validation_Report.md). Hardware evidence is stored under the checkpoint's `hardware-20261018T201612Z/`. Raw zero has now been correlated with zero publication; remaining SDK semantics require packet-matched lower-layer metadata or vendor clarification, not an arbitrary conversion change.

### Return-near measurement and restoration

The operator reported **10 local cycles**, despite the request for five. IDs **44..63** contain 20 new IAS records and 20 alternating MQTT publications: raw **173..209**, no zeros. All raw/bridge/encoded/MQTT values match in order; relative time-offset spread is about 4.98 ms. No queue drops, overwrites or missing records. Counts agree in aggregate with 20 physical transitions; individual action timestamps and absolute latency were not measured. Recovery near the board strengthens an environmental explanation without proving SDK validity semantics.

The final 63-record snapshot exposed a separate existing decoder defect: valid checksum FE on read-record 44 caused the following record-45 request to be discarded. Earlier snapshots ended at IDs 22/43. One bounded identical read-only retry retrieved the complete snapshot and was explicitly recorded. The collector defaults to no retries; `--read-retries 1` selects this recovery. No reset/NV/commissioning command was used. A separately justified source correction and exhaustive regressions are documented in the [checksum-boundary report](ZNP_Checksum_Boundary.md); that correction was not present during the physical LQI tests.

The temporary diagnostic application has now been removed. Full readbacks across restoration verify the original application SHA-256 `2af3bfc84eb28258ae2d191188929a99f88eb30c6edb0545997f87fb2e6a2aeb`, with every byte outside app0 unchanged. The unchanged host is running again on the retained network. No old NVS/counters were restored. The separate corrected default-OFF source subsequently received fresh operator approval for physical deployment. Its app-only write/readback, unchanged non-app0 storage, live checksum-boundary test, resumed host and six-event sensor smoke test all passed; see the separate correction report.

## Primary references

- [Espressif 1.x APS API](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32/api-reference/aps/esp_zigbee_aps.html), checked against installed 1.6.8 headers rather than the incompatible current 2.x API.
- [Component 1.6.8](https://components.espressif.com/components/espressif/esp-zigbee-lib/versions/1.6.8/readme).
- [IDF 5.5.5 radio metadata](https://github.com/espressif/esp-idf/blob/v5.5.5/components/ieee802154/include/esp_ieee802154_types.h), also checked against installed headers.
- [TI SWRA198 MT API](https://downloads.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/Z-Stack%20Monitor%20and%20Test%20API.pdf), printed pages 18-20. Local PDF/text and rendered page are retained in the private checkpoint; see [protocol audit](ZNP_AF_INCOMING_MSG_Audit.md) for the table caveat.

Current deployed state: corrected checksum-boundary application, diagnostics OFF, retained network, normal Windows lab running. The original application remains checkpointed. This does not change the LQI result: preserve raw zero, with SDK validity semantics unresolved.
