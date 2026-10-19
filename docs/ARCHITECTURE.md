# Source architecture

```text
Zigbee2MQTT zstack / Python serial utilities
    -> USB CDC or USB-UART (optionally VirtualBox passthrough)
    -> Arduino Serial / ZnpFrame
    -> ZnpBridge + ZnpNv -> EspNvStore (committed NVS)
    -> RadioBackend -> EspRadio + EspZdo
    -> ESP Zigbee SDK -> native ESP32-C6/H2 radio
```

- The sketch handles bounded serial input, bridge polling and RGB updates.
- `ZnpFrame` provides binary framing, validation and resynchronization.
- `ZnpBridge` dispatches commands and serializes AF/ZDO callbacks on the Arduino loop. `ZnpIdentity` defines the emulated TI version/reset indication.
- `ZnpNv`, `NvStore`, `EspNvStore` provide virtual TI records, configuration, live snapshot serialization and durable reset intent, with a native-test storage seam.
- `EspRadio` owns host-controlled direct SDK startup, known endpoints, groups, bounded callback queues, APS data, diagnostics and timers. It does not use Arduino ZigbeeCore's competing signal handler.
- `EspZdo` maps remote descriptors, management queries, binding, leave and network update to real callbacks.
- `ApsTxTracker` retains payloads and matches copied/out-of-order confirmations for concurrent distinct messages. The caller holds the radio lock.
- `StatusLed` drives GPIO8 on WeAct C6. Valid received application data triggers white; serial bytes do not. H2 defaults disabled until a board pin is selected.

Callbacks enqueue copies; the main loop writes binary Serial. AF acceptance means queued; a later confirmation supplies completion status. Per-device security/restore limitations are described in [serial compatibility](SERIAL_COMPATIBILITY.md).

The native peer runs production framing/dispatch/NV with a simulated radio. Hardware scripts drive unchanged published herdsman. The Windows launcher loads the unchanged Zigbee2MQTT entry module, starts local Mosquitto, and requests shutdown through the application's normal SIGINT handler.
