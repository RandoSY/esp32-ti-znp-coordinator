# ZNP AF incoming-message audit

Date: 2026-10-18. **PASS for the standard layout consumed by installed herdsman 10.10.0. No LQI serialization correction demonstrated.** This is an encoder/parser audit, not hardware validation of SDK measurement semantics.

## Sources and scope

Production: `EspRadio.cpp::indication:205`, `RadioBackend.h::ApsRx`, `ZnpBridge.cpp::emitIncoming:184`, `ZnpFrame.cpp::write`. Installed host: `build/zigbee2mqtt/node_modules/zigbee-herdsman/dist/adapter/z-stack/znp/definition.js:978-1017`, `zpiObject.js::fromUnpiFrame:59`, `buffalo/buffalo.js::readUInt8:34`, and `unpi/frame.js::fromBuffer`.

The field order/widths were checked against [TI SWRA198, MT API](https://downloads.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/Z-Stack%20Monitor%20and%20Test%20API.pdf), printed pages 18-20. The downloaded revision identifies itself as 1.178. Printed page 18 was rendered and visually inspected; the LQI row is one byte after WasBroadcast and before SecurityUse.

**Specification caveat:** that revision lists MacSrcAddr/Radius after Data in its attribute table while its standard packet diagram retains the 17-byte header and omits those trailing attributes. Installed herdsman also ends its definition at Data. Our firmware emits that installed-host layout and does not add trailing metadata. This is recorded as a broader compatibility limitation, not an LQI offset defect; no unrelated fields were added or changed here. The printed Data limit of 128 also differs from this implementation's tested 220-byte limit.

## Standard incoming message: 0x44 / 0x81

Offsets are zero-based. Frame offsets include `FE LEN CMD0 CMD1`; payload offsets start at GroupID. All multibyte fields below are little-endian.

| Field | Payload offset | Whole-frame offset | Width | Production source / installed host |
|---|---:|---:|---:|---|
| GroupID | 0 | 4 | 2 | `r.group` / UINT16 |
| ClusterID | 2 | 6 | 2 | `r.cluster_id` / UINT16 |
| SrcAddr | 4 | 8 | 2 | `r.src_addr` / UINT16 |
| SrcEndpoint | 6 | 10 | 1 | `r.src_ep` / UINT8 |
| DstEndpoint | 7 | 11 | 1 | `r.dst_ep` / UINT8 |
| WasBroadcast | 8 | 12 | 1 | `r.broadcast` / UINT8 |
| LinkQuality | **9** | **13** | **1** | `r.lqi` / **UINT8** |
| SecurityUse | 10 | 14 | 1 | `r.security` / UINT8 |
| Timestamp | 11 | 15 | 4 | `r.timestamp` / UINT32 |
| TransSeqNumber | 15 | 19 | 1 | bridge incrementing sequence / UINT8 |
| Len | 16 | 20 | 1 | `r.len` / UINT8 |
| Data | 17 | 21 | N | copied ASDU / exactly Len bytes |

Payload length is `17+N`; total serial frame size is `22+N`. The final FCS XORs LEN, CMD0, CMD1 and all payload bytes; SOF is excluded. N=0, 3 and 220 are tested. N=221 is rejected before a buffer overread. The fixed maximum frame payload is 250, and a 220-byte ASDU uses 237 bytes of it.

The serialized LQI is `uint8_t`, not RSSI. `LqiMetadata.h::lqiToByte` preserves the original `max(0,min(255,int))` policy exactly: 0..255 unchanged; negative ->0; above 255 ->255. There is no minimum-one rule or smoothing. The SDK documents no unavailable sentinel in this struct; the wire byte cannot distinguish real zero from a missing value already supplied as zero. Diagnostics retain the raw signed integer and range flag without claiming SDK validity.

Timestamp remains the baseline local `micros()/32`, not the SDK's reserved rx_time. TransSeqNumber remains the bridge's sequence, not the ZCL payload's transaction sequence. Neither was changed for this investigation; correlate both diagnostic ID and metadata rather than assuming these are native RF timestamps/sequence numbers.

## Extended incoming message: 0x44 / 0x82

This firmware does **not** emit AF_INCOMING_MSG_EXT. The host's definition was audited and exercised with independent synthetic fixtures only. AF_DATA_REQUEST_EXT is a separate transmit command and does not change that result.

| Extended field | Payload offset | Width |
|---|---:|---:|
| GroupID / ClusterID | 0 / 2 | 2 / 2 |
| SrcAddrMode / SrcAddr | 4 / 5 | 1 / 8 |
| SrcEndpoint / SrcPanID | 13 / 14 | 1 / 2 |
| DstEndpoint / WasBroadcast | 16 / 17 | 1 / 1 |
| LinkQuality / SecurityUse | **18** / 19 | **1** / 1 |
| Timestamp / TransSeqNumber | 20 / 24 | 4 / 1 |
| Len / Data | 25 / 27 | 2 / N |

Its LQI whole-frame offset is 22. It must not be parsed at standard offset 13. The host handles short addresses carried in the low two bytes of the eight-byte source field and has a separate huge-message retrieval path. Fixtures cover the inline-data path; production extended receive/huge-data behavior is NOT IMPLEMENTED/NOT RUN.

## Test evidence

`tests/test_lqi.cpp` drives the production bridge encoder with a synthetic radio and writes frames for independent host decoding. It tests raw values 0, 1, 15, 30, 142, 173, 254, 255, -1, -128, -129, 256, 32767, INT_MIN and INT_MAX at three lengths: **45 cases**. It independently checks nontrivial adjacent fields/endian order, payload preservation, frame size, FCS, oversized rejection and isolation across consecutive 173/0 packets.

`tests/fixtures/znp_af_lqi.json` contains six fixed synthetic fixtures, standard and extended at 0/173/255. Addresses/payloads are synthetic, not private captures. The fixtures follow the independently reviewed field layouts and are not generated by the production encoder during testing.

`tests/lqi_host.cjs` loads the installed, unchanged herdsman 10.10.0 Frame/ZpiObject parsers and ZStackAdapter callback. It checks every generated standard field/payload, standard/extended fixtures, zero/boundary passthrough and corrupted-FCS rejection. It also invokes unchanged Zigbee2MQTT 2.14.2 publication code with inert MQTT/state objects and temporary settings, demonstrating zero preservation, undefined handling and latest-value publication. No live controller or broker is started. The TS0203 IAS converter is exercised separately.

Existing logs independently correlate ten real standard wire frames with host parsed source/sequence/LQI and seven host-latest values with MQTT, all matching. They contain no zero raw frame, so they do not validate the source of the later zero reports.

In the authorized diagnostic near-location test, 22 live APS IAS indications had raw SDK LQI 81..209. All 22 raw values matched their bridge and encoded bytes and the MQTT publication sequence. No queue drops, overwritten records or missing IDs occurred. This is additional live encoder-to-publication evidence, without a separate serial wire tap; it does not establish the origin of zero, which was absent in this test. See [validation](LQI_Validation_Report.md) for physical count and timing limitations.

The bathroom test then supplied 21 new IAS indications (ring IDs 23..43), including **11 raw SDK zeros**. Every raw integer matched its bridge byte, encoded byte and ordered MQTT value. Thus reproduced zeros are not generated by signed conversion, byte placement or host substitution. Example ID 24 has status 0, raw/bridge/encoded/MQTT LQI all 0 and encoded transaction sequence 24. The RAM ring retained both phases, so phase counts are filtered by ID rather than assuming host restart clears it. SDK validity semantics remain outside this wire audit.

Run offline:

```powershell
python tests/run_lqi.py --compiler C:/Ruby33-x64/msys64/ucrt64/bin/g++.exe --output artifacts/lqi-tests
python -m unittest discover -s tests -v
```

On Linux, use `--compiler g++` (or omit it); the runner enables address/undefined-behavior sanitizers there. This session's Windows tests did not run sanitizers. Full commands/results and hardware NOT RUN items are in [validation](LQI_Validation_Report.md).
