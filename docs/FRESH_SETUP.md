# Starting from a fresh clone

This repository contains source, utilities and engineering evidence summaries. It does not contain the development machine's Zigbee network, installed applications, binaries or private backups. The Windows shortcuts operate an already configured local installation; cloning alone does not install Mosquitto or Zigbee2MQTT.

**Firmware workflow: Arduino IDE.** Clone the repository or use GitHub's **Code -> Download ZIP**, then open `Arduino/TI_ZNP_ESP32_RealBridge/TI_ZNP_ESP32_RealBridge.ino` and follow [Arduino setup](ARDUINO_SETUP.md). Keep the companion source files in that sketch folder. Python is used by the optional utilities/tests below; Arduino CLI is optional automation for the same IDE-compatible sketch.

```text
git clone https://github.com/RandoSY/esp32-ti-znp-coordinator.git
cd esp32-ti-znp-coordinator
python -m venv .venv
```

Activate the environment (`.venv\Scripts\Activate.ps1` on Windows, `source .venv/bin/activate` on Ubuntu), then:

```text
python -m pip install -r requirements.txt
python -m unittest discover -s tests -v
```

Install a C++17 compiler for native tests. Firmware uses the pinned **Arduino-ESP32 3.3.11** board package in Arduino IDE; install Arduino CLI as well if using the optional command-line build below. See [BUILDING](BUILDING.md) for developer commands, independent herdsman tests and exact dependencies. Optional CLI build without flashing:

```text
python tools/build_firmware.py --chip esp32c6 --transport native --flash-mb 4
```

Use `esp32h2` for an H2 board. Select actual flash capacity and USB transport. C6 GPIO8 is the tested RGB pin; H2 requires a board-specific LED pin. Diagnostics default OFF. The build helper never uploads firmware.

For an already running coordinator, retain its partition layout and network storage and use an operator-approved **application-only** update. See the [validation/rollback procedure](LQI_Validation_Report.md). Do not use full erase, merged images, or new network settings on a populated coordinator. A fresh unconfigured board needs the normal board-package bootloader/partition/application installation described in [Arduino setup](ARDUINO_SETUP.md).

Install Mosquitto and Zigbee2MQTT from their official instructions. The exercised versions are Mosquitto **2.1.2**, Zigbee2MQTT **2.14.2**, bundled herdsman **10.10.0**. Select serial adapter `zstack`, baud rate `115200`, hardware flow control disabled, and the actual COM/tty path. Existing network identity/key/channel settings must match the board. A fresh blank board needs its own new host-managed network; never copy the development network or its keys.

- [Official Zigbee2MQTT installation](https://www.zigbee2mqtt.io/guide/installation/)
- [Official Mosquitto downloads](https://mosquitto.org/download/)
- [Windows installation record and launcher requirements](INSTALLED_SOFTWARE.md)
- [Ubuntu/VirtualBox serial ownership](VM_USB_ZIGBEE2MQTT.md)

For the full installed-host LQI parser/publication tests, place an unmodified **2.14.2** Zigbee2MQTT checkout at `build/zigbee2mqtt`, install its frozen dependencies and build it according to its own instructions. Then run `python tests/run_lqi.py`. This invokes actual parser/publisher code with inert host objects; it does not start a controller, broker or serial connection.

When a board has the firmware and the host has released its serial port, `python tools/znp_vm_probe.py --port YOUR_PORT` performs four read-only identity/protocol checks. See [Getting Started](START_HERE.md) for the Python menu, RGB colors and physical sensor tests. Resume/join/startup utilities change live operation and must not be run casually against someone else's working network.

The project is an experimental TI ZNP serial personality backed by Espressif's Zigbee stack. It does not run TI firmware or establish Zigbee certification. See the [compatibility matrix](COMPATIBILITY_MATRIX.md) and [checksum-boundary correction](ZNP_Checksum_Boundary.md) before deployment.
