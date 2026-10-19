# Test the programmed WeAct ESP32-C6

The board tested here is an ESP32-C6FH4 with 4 MB flash, native USB Serial/JTAG, Windows **COM17**, and a WS2812 RGB LED on **GPIO8**. The firmware was fully erased and programmed, then updated while preserving its lab network. No external TI radio is used.

On the installed Windows lab, use `Stop-Lab.cmd` first and `Start-Lab.cmd` afterward. See [installed software and shutdown](INSTALLED_SOFTWARE.md). Use one serial owner at a time: close Zigbee2MQTT, Arduino Serial Monitor, and other terminals before either Python utility. Close the utility before restarting Zigbee2MQTT. These tools run directly on Windows; VirtualBox is needed only for the guest path.

## 1. Install the Python utility dependency

Windows PowerShell, from this project folder:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe tools\vm_menu.py
```

Ubuntu 22.04 terminal, from the copied/shared project folder:

```bash
sudo apt update
sudo apt install python3-venv usbutils
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python tools/vm_menu.py
```

Create a separate `.venv` inside the guest; a Windows virtual environment does not run on Linux.

## 2. Pass USB to Ubuntu, if using the VM

In the running VirtualBox VM, select **Devices -> USB -> the ESP32 USB Serial/JTAG device**. This transfers ownership from Windows to Ubuntu. The Windows COM17 port then becomes unavailable until USB is released by the VM.

In Ubuntu:

```bash
lsusb
ls -l /dev/serial/by-id/
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

Use the actual detected device, preferably `/dev/serial/by-id/...`. If permission is denied:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in before retrying. See [VirtualBox USB details](VM_USB_ZIGBEE2MQTT.md) for the controller/filter setup.

## 3. Run the numbered menu

1. **Menu 1:** identify the port. On Windows select COM17; in Ubuntu select the passed-through ESP32 tty.
2. **Menu 2:** run the four read-only checks. Expected: **4/4 PASS**, SYS_VERSION product **1**, revision **20240710**, and the board's IEEE address. The transcript is `znp_test_transcript.jsonl`.
3. **Menu 5:** read radio status and the exact diagnostic code. Expected healthy diagnostic: **`0x00000000`**.
4. After a reset, the LED is blue and device state is 1 while waiting for host startup. **Menu 6** resumes the already configured lab network. Expected: **device_state 9**, short address **`0x0000`**, a valid PAN/channel, and green RGB.
5. **Menu 7:** open local joining for 60 seconds and display events. The RGB turns cyan. Put the test sensor into pairing mode while this command is running.
6. A received **DEVICE ANNOUNCEMENT** with IEEE and NWK addresses demonstrates a real device joining the ESP32 network. A complete device interview/configuration requires the host application; the Python watcher does not perform an interview.
7. **Menu 8:** close joining; RGB returns green. **Menu 9:** watch announcements and received application packets for 60 seconds. Trigger the sensor, such as opening/closing a contact, after the host has configured reporting.

A freshly erased board without saved network configuration must first be commissioned by Zigbee2MQTT. Menu 6 does not invent a PAN or key and returns an error if none is configured.

## 4. Exact scripted commands

Windows:

```powershell
python tools/znp_vm_probe.py --port COM17 --jsonl znp-probe.jsonl
python tools/znp_lab.py --port COM17 status
python tools/znp_lab.py --port COM17 resume
python tools/znp_lab.py --port COM17 --jsonl sensor-events.jsonl join --seconds 60 --watch 60
python tools/znp_lab.py --port COM17 join --seconds 0 --watch 0
python tools/znp_lab.py --port COM17 watch --seconds 60
```

Ubuntu uses the same commands, replacing `COM17` with the detected `/dev/serial/by-id/...` path. With the Windows virtual environment, use `.\.venv\Scripts\python.exe` instead of `python` if it is not activated.

To send an explicit ZCL payload after discovering a device's address and endpoint:

```bash
python tools/znp_lab.py --port YOUR_PORT tx --dst 0x1234 --dst-ep 1 --src-ep 1 --cluster 0x0006 --data "01 01 02" --watch 5
```

This example is a Toggle command for an On/Off device, not a contact-sensor command. Replace its address, endpoint, cluster, and complete ZCL bytes for the actual device. `AF request accepted` means queued; **TX CONFIRM status `0x00`** is the stack's delivery result. A received application response provides stronger evidence of bidirectional operation.

## 5. RGB meanings

| RGB | Meaning |
|---|---|
| Breathing blue | Serial ready; waiting for host network startup |
| Blinking purple | Zigbee initialization / formation |
| Steady green | Coordinator ready, including idle |
| Cyan | Joining open; automatically returns to green when the window expires |
| White flash (120 ms), then previous color | Valid received Zigbee application data, including door/window reports |
| Brief amber | Rejected command, bad checksum, or dropped queued event |
| Blinking red | Radio initialization or persistent-storage error; use menu 5 |

White indicates received application data, rather than USB polling or a confirmed transmission. Battery reports and other application messages also flash white. It can briefly override green or cyan; warnings and faults take priority. With menu 9 running, operate the enrolled sensor: each received report should produce a white flash and an `RX` event. The LED does not distinguish open from closed. Full Zigbee2MQTT converts this sensor's reports to `contact: true` (closed) and `false` (open).

A diagnostic such as `0x030B0102` identifies phase 3 (cluster registration), endpoint 11, SDK error `0x0102`. The utility decodes this automatically.

## 6. Test with Zigbee2MQTT

For a fresh public clone and a new, unconfigured coordinator, use [Getting Started's complete host/configuration procedure](START_HERE.md#first-time-setup-new-board-and-new-network). The generator below requires an existing private `lab-network.json`, which is deliberately excluded from GitHub; it is not the first-start path for a new board. Never replace an existing network's saved settings with generated identities or keys.

Exit the Python utility. Use a dedicated Zigbee2MQTT data directory with `serial.adapter: zstack`, 115200 baud and `rtscts: false`. To preserve the already tested lab network, generate a configuration using its local, private network file:

```bash
python tools/make_z2m_config.py --port /dev/serial/by-id/YOUR_ESP32 --output lab-zigbee2mqtt/configuration.yaml
```

Run that data folder with your Zigbee2MQTT installation. The generated JSON-formatted file is valid YAML. It uses `mqtt://localhost` by default; provide `--mqtt` for another broker. Keep the generated configuration and `artifacts/esp32c6/lab-network.json` out of GitHub because they contain the lab key.

The developer library tests use unmodified **zigbee-herdsman 11.1.0** and do not start MQTT. Separately, the installed full **Zigbee2MQTT 2.14.2** uses bundled **herdsman 10.10.0** with **Mosquitto 2.1.2**; real contact/battery MQTT publishing passed on Windows. See [build/test instructions](BUILDING.md) and [RF evidence](RF_ACCEPTANCE.md).

## Evidence to keep

Record board/core versions, host version, serial path, probe transcript, diagnostic code, pairing/interview result, contact reports, and a repeat after reset/power cycling. Protocol startup, actual pairing, device interview, and sensor reporting are separate results.
