# Valid checksum 0xFE: serial decoder correction

Status: **corrected in source and on C6 hardware; native/build/hardware-boundary/sensor smoke checks PASS**. Found independently of the LQI measurement issue on 2026-10-18. No production compatibility/release version was incremented.

## Failure and evidence

`FrameDecoder::input` in `Arduino/TI_ZNP_ESP32_RealBridge/ZnpFrame.cpp` previously accepted a valid frame, cleared its state, then treated any checksum byte `0xFE` as the start of another frame. If the next request followed within the 750 ms incomplete-frame timeout, its actual SOF was consumed as an invalid length and the request could be discarded.

The final diagnostic snapshot crossed this boundary. Read-record 44 has the valid bytes:

```text
FE 05 27 F1 01 2C 00 00 00 FE
```

Read-record 45 immediately afterward timed out. A bounded retry of that identical read succeeded. The saved 63-record snapshot explicitly records this timeout; no coordinator reset, NV operation, commissioning request or fabricated measurement was used to recover it.

The independent native reproducer feeds the above frame followed immediately by valid record 45. Original decoder: **FAIL, expected 2 accepted frames, actual 1**. It uses production `ZnpFrame.cpp`, without an ESP SDK or serial driver. The exhaustive regression likewise failed specifically at valid FCS `0xFE` before the source change.

## Minimal correction

Retain whether the checksum matched before clearing decoder state. Reuse `0xFE` as a possible new SOF **only when the checksum was invalid**. Valid checksums, including `0xFE`, now leave the decoder waiting for the next actual SOF.

No command definitions/replies, encoder bytes, LQI conversion, radio callback, commissioning, security, persistence, partitions, transport settings or LED policy changed. The separate LQI diagnostics remain OFF by default.

## Validation

| Check | Result |
|---|---|
| Original manual reproducer and exhaustive boundary test | FAIL before correction, as reproduced |
| All 256 valid FCS values followed immediately by another valid frame | PASS after correction |
| Invalid FE checksum reused as the next SOF | PASS; existing recovery preserved |
| Invalid non-FE checksum and fresh SOF | PASS |
| Truncated-frame timeout recovery | PASS |
| Maximum 250-byte payload containing FE bytes | PASS |
| Full native dispatch/NV/APS/LQI suite and simulated herdsman reset/resume | PASS |
| Installed herdsman 10.10.0/Z2M 2.14.2 isolated LQI tests | PASS |
| Python collector/parser tests, 17 | PASS |
| C6/H2 corrected firmware builds | PASS; C6 flash 727602/static RAM 40492; H2 flash 732717/static RAM 38996 |
| Corrected application on physical COM17 | PASS; explicit operator authorization, app-only write/readback, all non-app0 bytes unchanged |
| Live valid-FE frame followed immediately by PING | PASS; both expected responses, no retry, no parser FCS errors |
| Existing paired UZ-6D, 3 cycles after corrected deployment | PASS; six MQTT states false/true repeated three times, LQI 163..188 |

Run `python tests/run_native.py --host` after installing the documented independent host dependency, and `python -m unittest discover -s tests -v`. The runner includes `tests/test_framing.cpp`. Standalone reproduction:

```bash
g++ -std=c++17 -I Arduino/TI_ZNP_ESP32_RealBridge tests/repro_fcs_fe.cpp Arduino/TI_ZNP_ESP32_RealBridge/ZnpFrame.cpp -o build/repro-fcs-fe
build/repro-fcs-fe
```

Create `build/` first on a fresh clone. On Windows use the compiler environment shown in BUILDING; include its runtime DLL directory on PATH.

## Deployment and relation to LQI

The working board was restored to the original application after the authorized LQI experiment. Readback verifies the original image, with all non-app0 bytes unchanged across rollback. That original application retains this decoder defect. The corrected source was then separately authorized and flashed application-only. Its own hardware results, independent of the earlier LQI experiment, are recorded below.

The defect did **not** create the bathroom zero values: those were already zero in the SDK APS field. All 63 captured LQI values match through the bridge, encoded field and ordered MQTT observations. See [investigation](LQI_RSSI_Investigation.md) and [validation/rollback](LQI_Validation_Report.md).

The corrected default-OFF C6 build uses 4 more reported sketch bytes than the baseline, with unchanged static RAM. Runtime stack high-water and timing remain unmeasured. SDK versions, partitions and serial settings are unchanged.

If approved for live validation, deploy only the corrected C6 application at `0x10000`, retaining the existing NVS/Zigbee storage and host configuration. Verify normal resumed startup, paired sensor reports and LEDs. Exercise the valid-FCS-FE boundary using read-only requests, then verify the following command succeeds without a retry. Keep the known-good application and restore only app0 if needed; never restore old security counters or factory-reset a populated network.

## Authorized corrected-build hardware result

On 2026-10-18 at 21:43 local time, the corrected default-OFF C6 application (SHA-256 `23e0de9c8bbfa5cadecea8fa114697d1ea193fc42b59a6e2eb9757dccd8fac42`) was verified by full readback. Every byte outside app0 matched the fresh pre-write image. The original known-good application remains available for rollback; no old NVS or counters were restored.

The read-only hardware utility sent `FE 05 27 F1 01 2C 00 00 00 FE` immediately followed by PING in one serial write. With diagnostics OFF, the first query correctly returned its normal unsupported-command response (`6000`); PING returned `6101` without retry. This intentionally unsupported query can briefly trigger the protocol-warning LED; it makes no NV/network change. `tools/znp_checksum_boundary.py` preserves this exact test and requires explicit operator-approved exclusive port access.

Zigbee2MQTT logged `started (resumed)` and retained the UZ-6D. The operator performed three cycles; six fresh MQTT publications were exactly `false,true,false,true,false,true`, LQI `[188,168,173,173,183,163]`. This is a six-event smoke test, not a stress/loss-rate certification. No new LED confirmation was supplied in the completion reply; RGB code is unchanged and earlier white/green behavior was confirmed. Fresh battery RF reporting, H2 hardware, long-duration load and runtime high-water measurements remain NOT RUN.

Evidence is private under `artifacts/lqi-investigation/20261018T200035Z/hardware-20261018T201612Z/framing-hardware-20261019T014107Z/`: `storage-verification.json`, `checksum-boundary.json`, `contact-analysis.json`, immutable contact capture and flash logs. Raw flash/host backups are excluded from publication.
