# Arduino firmware setup

Pin **Arduino-ESP32 3.3.11**, including its SDK libraries. Open `Arduino/TI_ZNP_ESP32_RealBridge/TI_ZNP_ESP32_RealBridge.ino`; keep companion C++ files in the sketch directory.

| Setting | Tested C6 value |
|---|---|
| Board | ESP32C6 Dev Module, physical WeAct ESP32-C6FH4 |
| Zigbee mode | ZCZR (coordinator/router) |
| Partition scheme | Zigbee ZCZR |
| Flash size | 4 MB |
| USB CDC On Boot | Enabled for native USB |
| Core debug level | None; binary serial must remain free of logging |
| Port | COM17 on this Windows host |
| RGB | WS2812 GPIO8, low brightness |

For UART0 through an external communication chip, disable CDC on boot and use the actual serial connector. Match flash capacity to the board. H2 builds use ZCZR; its RGB defaults disabled until the board's pin is supplied.

The [build helper](BUILDING.md) records FQBN and image hashes. Both targets compile with the pinned core. C6: 727598 bytes sketch, 40492 bytes static RAM. H2: 732713 bytes sketch, 38996 bytes static RAM. Only C6 has physical acceptance here.

The C6 was fully erased for initial programming. Later app-only updates preserved the paired network. New boards need the generated bootloader, partition table and application at their correct offsets; use Arduino Upload or generated flash arguments. Do not erase a paired board unless intentionally resetting its network.

After reset, serial is ready and blue indicates waiting for host-controlled startup. Zigbee2MQTT or Python menu 6 resumes the saved network. Green means ready/idle; white pulses indicate received Zigbee application data. See [Getting Started](START_HERE.md).
