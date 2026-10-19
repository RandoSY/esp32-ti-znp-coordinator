# Ubuntu 22.04 / Oracle VirtualBox: ESP32-C6/H2 and Zigbee2MQTT

## 0. Isolation

Use a **separate test coordinator and Zigbee2MQTT data folder**. Keep your working TI CC2652P / Raspberry Pi IAS installation untouched. Stop Zigbee2MQTT before running the standalone probe; serial ports have one owner.

## 1. Program the board on the Windows host

Open the multi-tab sketch in Arduino IDE; flash with ZCZR Zigbee enabled and USB CDC on boot for native-USB boards. **No human-readable boot banners** should appear on the ZNP protocol port. Native USB CDC normally appears as a COM port on Windows and as `/dev/ttyACM*` on Ubuntu. A USB-UART adapter may appear as `/dev/ttyUSB*` instead.

## 2. Pass the actual USB device into the guest

In your VM software's **USB devices** menu, select/connect the ESP32-C6/H2 device **to the Ubuntu guest**, disconnecting it from Windows. This is USB-device passthrough, *not* a virtual serial port pairing or COM emulation. Exact menu names depend on VMware, VirtualBox or other hypervisors. If the VM provider lacks USB passthrough, this method won't work until enabled.

### Oracle VirtualBox on Windows

1. Shut down the Ubuntu VM. In **Settings -> USB**, enable the USB controller and select **USB 3.0 (xHCI)**; it also supports USB 2.0 devices.
2. Optionally add a USB filter using the **+** button and select the actual ESP32 board. Use its VID/PID and, if available, serial number. Avoid a blank filter that could capture unrelated devices.
3. Start Ubuntu. In the VM window choose **Devices -> USB -> your ESP32 device**. After creating a filter on Windows, unplug/reconnect the board if capture does not occur.
4. After flashing or entering the bootloader, check the USB selection again: the device identity or guest tty path can change.

Use **USB**, rather than VirtualBox's **Serial Ports** settings, for this workflow. Guest Additions are useful for shared folders but are not required for USB capture. With older VirtualBox releases, check that release's USB requirements; do not assume an Extension Pack is required by every version.

Reference: [Oracle VirtualBox manual, USB settings](https://download.virtualbox.org/virtualbox/7.2.8/UserManual.pdf#page=81).

Ubuntu guest checks:

```bash
lsusb
sudo dmesg | tail -50
ls -l /dev/serial/by-id/ 2>/dev/null || true
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
python3 --version
```

The ESP32's VID/PID or product name should appear and one tty should exist. If not, reconnect the USB device to the guest; unplug/replug if necessary. It is NORMAL for the Windows COM port to disappear while the guest owns the USB device.

Permissions (for a non-root local test):

```bash
groups
# If 'dialout' is absent:
sudo usermod -aG dialout "$USER"
# Log out/in (or reboot guest) before retrying
```

## 3. Start with a non-mutating protocol probe

Install `pyserial` inside a Python virtual environment:

```bash
sudo apt update
sudo apt install python3-venv python3-pip usbutils
# Run the following from the extracted project directory:
python3 -m venv .venv
. .venv/bin/activate
python -m pip install pyserial
python tools/vm_menu.py                      # numbered menu, easiest
# Or for repeatable scripted checks:
python tools/znp_vm_probe.py --list
python tools/znp_vm_probe.py --port /dev/ttyACM0 --jsonl test-session.jsonl
```

Use the actual tty or stable `/dev/serial/by-id/...` path. The probe sends **only four read-only requests**: SYS_PING, SYS_VERSION, SYS_GET_EXTADDR, UTIL_GET_DEVICE_INFO; all raw responses and latencies are logged. A fresh ESP32 native USB port can need a short reconnection after flashing; if it fails, inspect guest dmesg and select the new port.

## 4. Start Zigbee2MQTT with its native Z-Stack driver

Example portion of `configuration.yaml`, for a **dedicated test instance**:

```yaml
mqtt:
  server: mqtt://localhost
  base_topic: znp_esp32_lab
serial:
  port: /dev/ttyACM0
  adapter: zstack
  baudrate: 115200
  rtscts: false
advanced:
  log_level: debug
```

Preserve any other required settings; do **not** overwrite production YAML. Ensure the standalone Python probe has exited. Start Zigbee2MQTT using **your existing installation method**, and capture its logs, for example:

```bash
# If your installation is a systemd service:
sudo journalctl -fu zigbee2mqtt
# In another terminal, if needed:
sudo systemctl restart zigbee2mqtt
```

Other installations use Docker or `npm start` and have different log commands. If you use Docker, the USB device must ALSO be mapped into the container (e.g. `--device=/dev/ttyACM0`) after guest passthrough.

**Current evidence:** full Zigbee2MQTT 2.14.2 starts and publishes contact/battery reports on Windows COM17. The Ubuntu/VirtualBox path needs its own test. The firmware presents a TI ZStack3x0 serial personality; broader backup/security limits are listed in [the compatibility matrix](COMPATIBILITY_MATRIX.md). Keep the second host's network settings aligned with the saved board network. The Ubuntu installation is being handled separately; this session installed only the [Windows lab](INSTALLED_SOFTWARE.md).

## 5. Diagnose and send useful evidence

1. Capture `test-session.jsonl` from the probe.
2. Capture the relevant Zigbee2MQTT `debug` startup log (remove MQTT credentials / network keys before sharing).
3. Record Arduino-ESP32 version, board variant, VM provider, Ubuntu version and detected tty path.
4. Log the **first request command that fails** and its host expectation. Implement it with a real backed state change or explicit error (never fake a successful NV write).

Diagnostic decision tree:

- No `/dev/ttyACM*` or `/dev/ttyUSB*`: VM USB passthrough or cable problem.
- Permission denied: serial ownership, `dialout`, or Docker mapping.
- Probe times out for SYS_PING: firmware/USB CDC selection, binary port corruption, or boot state.
- Probe passes, Zigbee2MQTT fails before network commissioning: TI compatibility/API mismatch, *not* proof of RF failure.
- Zigbee2MQTT starts but sensors fail to join: separate Zigbee commissioning, security, antenna, channel, RF and persistent-state tests.

Avoid running the standalone probe concurrently with Zigbee2MQTT. Do not open Arduino Serial Monitor or any second terminal on the same serial port while the service owns it.
