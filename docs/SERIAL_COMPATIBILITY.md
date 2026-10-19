# TI serial personality and its limits

The firmware presents the TI ZNP interface expected by Zigbee2MQTT's unmodified `zstack` driver. It implements framing, requests/replies, asynchronous events, durable virtual TI NV, host-controlled startup, descriptors, discovery, and APS traffic. Espressif's stack and native radio execute the real network operations.

The compatibility identity is transport 2, product 1, version 2.7.1, revision 20240710. This selects the host's ZStack3x0 path; it does not mean TI firmware executes on ESP32.

## Verified host paths

Unmodified herdsman 11.1.0 passed actual C6 adapter/controller startup, commissioning, reset/resume and sensor interview. Full unmodified Zigbee2MQTT 2.14.2, bundled herdsman 10.10.0, also started/resumed, configured the Wing TS0203 UZ-6D, and published contact and battery reports through Mosquitto 2.1.2 on Windows COM17. No driver patches or host-side serial translation proxy were used.

The implementation source reference is [herdsman commit e399aec](https://github.com/Koenkk/zigbee-herdsman/tree/e399aec41602622de6bfe56e5f465fec9c13fdbd/src/adapter/z-stack). Its command definitions, startup manager, NV helper, and aligned structures define the tested host expectations. Published packages are exercised directly by the tests.

## State and real effects

OSAL/NV_EXT records live in committed ESP NVS independently of the SDK's Zigbee NVRAM. Network parameters are applied before formation. The host-visible NIB and network key/counter records are refreshed from actual stack state. Reset uses a durable erase intent.

The real stack registers 15 known herdsman endpoints before startup. Local descriptors describe those endpoints; AF registration accepts matching descriptors. Group addition checks actual membership. Remote ZDO uses SDK callbacks. APS acceptance and delivery confirmation are separate; confirmation matching uses destination, endpoints and payload bytes because the SDK may copy the ASDU.

## Incomplete compatibility

Virtual TI address-manager, trust-center/link-key and security-manager tables do not yet export complete per-device SDK security state. A generated network-only backup is **not a complete migration/restore implementation**. Per-device keys/counters and key rotation need further translation and acceptance. An arbitrary successful NV record write does not necessarily apply that record to SDK internal tables.

Other limits include arbitrary dynamic endpoints, inter-PAN/touchlink, full MT coverage, and indistinguishable identical concurrent AF requests. Up to 16 distinct packets can be pending, with 220-byte payloads. Unsupported paths fail explicitly.

The Python probe establishes only its four exchanges. Device operation, application startup, persistence and restoration require separate evidence. See [compatibility matrix](COMPATIBILITY_MATRIX.md) and [RF acceptance](RF_ACCEPTANCE.md). The current tests do not establish complete TI Zigbee 3.0 equivalence or certification.
