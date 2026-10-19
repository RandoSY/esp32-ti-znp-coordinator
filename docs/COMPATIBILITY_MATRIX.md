# Compatibility matrix

The advertised personality is TI Z-Stack 3.x, product 1, compatibility revision 20240710. This is an emulated identity; the firmware executing on the radio is Espressif's stack.

| Function | Implementation / evidence |
|---|---|
| FE framing, fragmented input, FCS rejection, resync | Native regression tests passed |
| PING, VERSION, EXTADDR, device info | Passed actual hardware and unmodified host startup; associated-device count is not authoritative |
| OSAL NV and NV_EXT | Durable ESP NVS records, bounds and failure tests; live NIB/network key/counter projection |
| Host-controlled formation, reset, resume | Passed actual host commissioning and restart; durable erase intent |
| Application endpoints | 15 standard herdsman endpoint descriptors registered in real stack; arbitrary dynamic descriptors rejected |
| Groups | Actual APS membership verified; uses a pinned Espressif test/certification API |
| AF application data | Real TX/RX with asynchronous confirms; copied-ASDU matching supports distinct concurrent payloads |
| TX limits | 16 pending, 220-byte payload maximum; indistinguishable identical requests to the same destination/endpoints return busy |
| Discovery/ZDO | Address, node, power, simple/active descriptors, bind/unbind, neighbor/routing/binding queries, leave, join, network update translated to SDK |
| Local descriptors / device announcements | Passed coordinator descriptors and real sensor interview |
| RGB | Green ready/idle, 120 ms white receive pulse; see Getting Started for all states |
| Unmodified herdsman 11.1.0 | Actual adapter/controller startup, sensor interview, contact reports passed |
| Full Zigbee2MQTT 2.14.2 / herdsman 10.10.0 | Passed Windows COM17 startup/resume, MQTT contact and battery publishing |
| Same-board persistence | Reflash/reboot resumed and retained sensor communications |
| Full TI device/security backup and restoration | **Incomplete**: address-manager/device-link-key tables are not exported from actual SDK state; network-only backup is not a full migration backup |
| Key rotation/update ID projection | Not fully implemented/validated |
| Inter-PAN/touchlink, arbitrary AF endpoints, complete MT command set | Unsupported/incomplete |
| H2 hardware, UART transport, Ubuntu/VirtualBox path | Build/instructions only; no corresponding physical acceptance yet |
| TI Zigbee 3.0 equivalence/certification | Not established by these tests |

See [RF acceptance](RF_ACCEPTANCE.md) for evidence and [serial compatibility](SERIAL_COMPATIBILITY.md) for the emulation boundary. Unsupported operations must report failure rather than fictitious RF or storage success.
