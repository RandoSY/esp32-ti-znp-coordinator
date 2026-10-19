# Build and developer verification

The current firmware is pinned to **Arduino-ESP32 3.3.11**, including its Zigbee SDK 1.x API and native ZCZR radio libraries. It uses the SDK directly so that TI serial requests control startup and application traffic. It does not link Arduino's `ZigbeeCore` task/signal handler.

## Firmware

Install Arduino CLI and the Espressif board package, then pin the version:

```text
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
python tools/build_firmware.py --chip esp32c6 --transport native --flash-mb 4
python tools/build_firmware.py --chip esp32h2 --transport native --flash-mb 4
```

For a board using a USB-to-UART chip connected to UART0, build with `--transport uart`. Select the real flash capacity. The tested WeAct C6FH4 on COM17 uses **native** USB and **4 MB** flash.

The build helper creates `artifacts/<chip>/` images and a `build.json` with FQBN, installed core metadata, and SHA-256 hashes. It compiles without uploading. Arduino IDE settings are in [ARDUINO_SETUP.md](ARDUINO_SETUP.md).

RGB is enabled on GPIO8 for C6 by default. Other LED pins can be supplied as `ZNP_RGB_PIN`; H2 defaults to disabled until a board-specific pin is selected. RGB uses Arduino's addressable-LED driver and never prints to the ZNP port.

One isolated SDK dependency uses `test/esp_zigbee_test_utils.h` for direct APS group-table operations, needed for the host's reserved Green Power endpoint 242. Espressif marks that API unstable; this is one reason to pin and rebuild against the recorded core rather than silently upgrade it. A successful operation is checked against actual APS table membership.

## Native tests without hardware

Ubuntu:

```bash
sudo apt install g++ python3
python3 tests/run_native.py
python3 -m unittest discover -s tests -v
```

The native runner uses address/undefined-behavior sanitizers on Linux. On Windows it uses the selected C++ compiler without those sanitizers. For example, on the development machine:

```powershell
python tests/run_native.py --compiler C:/Ruby33-x64/msys64/ucrt64/bin/g++.exe
python -m unittest discover -s tests -v
```

## Unmodified Zigbee2MQTT host-library test

Install Node.js/npm and the exact test dependency:

```text
npm install --prefix build/host zigbee-herdsman@11.1.0 --no-audit --no-fund
python tests/run_native.py --host
```

The test runs the published, unmodified `ZStackAdapter` against production C++ frame/dispatch/NV code. The peer uses a simulated radio and disk-backed records. It verifies fresh commissioning, restart/resume, TI identity, configured PAN/channel/key, joining close, and network backup. Transcripts go under `artifacts/host-compatibility/`. This is a host-protocol check, not RF evidence.

## Actual board checks

The following commands take ownership of the selected port and change the isolated lab network. They require the board firmware and pyserial dependency to be installed:

```text
python tools/znp_vm_probe.py --port COM17
node tests/hardware_startup.cjs COM17
node tests/hardware_controller.cjs COM17
```

`hardware_startup.cjs` records private lab options in `artifacts/esp32c6/lab-network.json` on first use. It runs the actual adapter against the real radio, compares the live network key with the host's configured key, and tests reset/resume. `hardware_controller.cjs` starts the full herdsman controller, reads coordinator endpoint descriptors, and saves the controller database and backup in a separate test directory.

For an interactive physical-device test:

```text
node tests/hardware_controller.cjs COM17 180
```

Wait for `JOINING OPEN` before putting the sensor into pairing mode. Watch `DEVICE JOINED`, `DEVICE INTERVIEW`, and `DEVICE MESSAGE` output. This starts herdsman, not the MQTT application. Do not run Python utilities or Zigbee2MQTT simultaneously on the same port.

Keep generated logs, backups, network keys, native executables, node_modules, and build outputs out of GitHub. The supplied `.gitignore` excludes them.
