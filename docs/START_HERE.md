# Getting started: ESP32 ZNP lab

**BETA, with successful tests. Arduino IDE is the primary firmware workflow.** This guide takes a new board from source to contact reports and explains how to operate the already installed Windows lab. C6 hardware and C6/H2 compilation have passed; broader device coverage and H2 RF testing remain unfinished.

The ESP32 presents a TI Z-Stack 3.x ZNP serial personality to the unmodified host `zstack` driver. Its own native 802.15.4 radio runs the Zigbee network. It does not execute TI firmware or reproduce a CC2652P board's amplifier/range.

| Your situation | Start here |
|---|---|
| New, unconfigured board and new test network | Follow the first-time setup below |
| Our already installed Windows lab | Jump to [Start the installed Windows lab](#start-the-installed-windows-lab); do not create another network |
| Board already paired with devices | Keep its host configuration and saved network; read [Safe updates and rollback](#safe-updates-and-rollback) before programming |
| Testing in Ubuntu/VirtualBox | Follow the same firmware/protocol checks; [transfer USB ownership](VM_USB_ZIGBEE2MQTT.md) before using the guest |

## First-time setup: new board and new network

**1. Gather the hardware and software.**

Use the pictured [WeAct Studio ESP32-C6-MINI](../README.md), a USB-C **data** cable, and a computer. The tested C6 has 4 MB flash, native USB and RGB on GPIO8. H2 is also a compile target; match its actual board/flash/LED pin rather than assuming the C6 settings apply unchanged.

For firmware, install [Arduino IDE](https://www.arduino.cc/en/software) and download this repository using **Code -> Download ZIP** or Git. Extract it before opening the sketch. Python 3 with `venv` is optional for the included utilities. Zigbee2MQTT also needs Node.js and an MQTT broker; tested versions are Node **22.20.0**, Zigbee2MQTT **2.14.2** and Mosquitto **2.1.2**. Its package selects pnpm **11.26.0**. Arduino CLI and a separate C++ compiler are developer/CI tools, not requirements for using Arduino IDE.

**2. Install the pinned ESP32 board package and open the sketch.**

In Arduino IDE **Preferences -> Additional Boards Manager URLs**, add:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Open **Boards Manager**, find **esp32 by Espressif Systems**, select **3.3.11** and install it. Keep this tested version rather than accepting an automatic SDK upgrade. Open:

```text
Arduino/TI_ZNP_ESP32_RealBridge/TI_ZNP_ESP32_RealBridge.ino
```

Keep every companion `.cpp` and `.h` file in that folder; Arduino IDE compiles the multi-file sketch. No separate Zigbee library installation is needed beyond this board package. These settings come from its installed `boards.txt`:

| Arduino IDE setting | WeAct C6 value |
|---|---|
| Board | ESP32C6 Dev Module |
| Zigbee Mode | Zigbee ZCZR (coordinator/router) |
| Partition Scheme | Zigbee ZCZR 4MB with spiffs |
| Flash Size | 4MB (32Mb) |
| USB CDC On Boot | Enabled |
| Core Debug Level | None |
| Erase All Flash Before Sketch Upload | Disabled |
| Port | The board's actual COM port; COM17 is our development machine's assignment |

Click **Verify** first. A missing `ZIGBEE_MODE_ZCZR` error means the Zigbee Mode selection is wrong; missing ESP Zigbee headers usually means the core/package selection is wrong. See [Arduino setup](ARDUINO_SETUP.md) and [Espressif's official board-package instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).

**3. Program only the new, unconfigured board.**

Close Serial Monitor, Zigbee2MQTT and every other program using that port. Click **Upload**. This installs the board-package bootloader, selected partition table and application on a new board. If connection stalls, hold **BOOT**, press/release **RESET**, release **BOOT**, select the detected port and retry. Reset after uploading if it remains in download mode. Do not repeat this initial-install procedure on a populated coordinator; use the safe-update procedure below.

**Success:** Arduino IDE reports upload complete and, after normal boot, the board shows blue while waiting for host-controlled Zigbee startup. Blue is expected at this point. Serial Monitor is not a text console for this binary ZNP interface; leave it closed.

**4. Check serial communication before commissioning.**

From the extracted project root in Windows PowerShell:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe tools/znp_vm_probe.py --list
.\.venv\Scripts\python.exe tools/znp_vm_probe.py --port COM17
```

Replace COM17 with your actual port. **Success: 4/4 PASS**, product 1, version 2.7.1, revision 20240710. This is the emulated TI identity, not the Arduino core version. It verifies serial framing/identity, not RF reception. A blank board has no configured network yet, so do not use `resume` as its commissioning step. For Ubuntu, create its own virtual environment and use the detected `/dev/serial/by-id/...` path; see [Python/Ubuntu instructions](TESTING.md).

**5. Install the host applications and start the MQTT broker.**

Use the official [Mosquitto downloads](https://mosquitto.org/download/) and [Zigbee2MQTT installation instructions](https://www.zigbee2mqtt.io/guide/installation/) for your operating system. The [Windows instructions](https://www.zigbee2mqtt.io/guide/installation/05_windows.html) cover Node/Corepack/pnpm. For the tested source version, from this project root, with Git and Node installed:

```text
corepack enable
git clone --depth 1 --branch 2.14.2 https://github.com/Koenkk/zigbee2mqtt.git build/zigbee2mqtt
cd build/zigbee2mqtt
pnpm install --frozen-lockfile
pnpm run build
cd ../..
```

Do this only for a fresh installation; do not clone over or replace an existing installation. `corepack enable` may require installation-directory permissions on Windows; follow the linked official installation instructions if needed. These commands do not install Mosquitto or register a service.

Start your MQTT broker first. For a local foreground test, when `mosquitto` is on PATH and no broker/service already owns port 1883:

```text
mosquitto -v -p 1883
```

Leave that terminal open. With no configuration file, Mosquitto's default listener is local-only; see its [official manual](https://mosquitto.org/man/mosquitto-8.html). If your installation already runs a broker service, use it instead of starting a duplicate. The shortcuts supplied here require our separately prepared portable Windows installation; a fresh clone does not install it.

**6. Configure only the new test network.**

Create a private `lab-zigbee2mqtt` directory in this project and save the following as `lab-zigbee2mqtt/configuration.yaml`. This example is **only for a new, unconfigured coordinator**. For an existing network, retain its real configuration, PAN/key/channel and database instead; never replace them with `GENERATE`.

```yaml
version: 5
mqtt:
  base_topic: esp32_znp_lab
  server: mqtt://127.0.0.1:1883
serial:
  port: COM17
  adapter: zstack
  baudrate: 115200
  rtscts: false
advanced:
  channel: 20
  network_key: GENERATE
  pan_id: GENERATE
  ext_pan_id: GENERATE
frontend:
  enabled: true
  host: 127.0.0.1
  port: 8080
```

Set the actual serial path; in Ubuntu use its stable tty path. For a remote/authenticated broker, supply your private MQTT endpoint/credentials instead of this loopback example. Zigbee2MQTT replaces `GENERATE` values with saved network settings; keep the resulting file private and retain it. See its [network settings documentation](https://www.zigbee2mqtt.io/guide/configuration/zigbee-network.html). The included `make_z2m_config.py` is for an existing private lab-network file and is not needed for this fresh-network example.

Close the Python utility and Serial Monitor. In a new PowerShell terminal **from this project root**:

```powershell
$env:ZIGBEE2MQTT_DATA = (Resolve-Path ./lab-zigbee2mqtt).Path
Set-Location ./build/zigbee2mqtt
pnpm start
```

Or in Ubuntu, from the project root after transferring USB ownership:

```bash
export ZIGBEE2MQTT_DATA="$PWD/lab-zigbee2mqtt"
cd build/zigbee2mqtt
pnpm start
```

**Success:** the broker connects, Zigbee2MQTT reports that it started, http://127.0.0.1:8080 opens on the computer running it, and the board settles to green. The first start forms the new network; subsequent starts should resume it. The `localhost` address inside a VM refers to that VM, not Windows. If pairing/network mismatch errors appear on an already used board, stop and check saved configuration rather than erasing it.

**7. Pair one device, then verify real reports.**

In the Zigbee2MQTT web UI, enable **Permit join**, put a **new test device** into its manufacturer's pairing mode and wait for a successful interview/configuration. Close joining afterward. Already paired sensors do not need this step. Cyan means joining is open; green means ready; white flashes mark received application data.

For the tested [UZ-6D / Wing TS0203 door contact](TEST_SENSOR.md), open/close the magnet. **Success:** the UI/MQTT changes between `contact: false` (open) and `contact: true` (closed), and activity flashes white. Rename it `UZ-6D_Door` if you want the topic/utility defaults used in our lab. Then a Mosquitto client on PATH can watch it without taking the serial port:

```text
mosquitto_sub -h 127.0.0.1 -p 1883 -t esp32_znp_lab/UZ-6D_Door
```

Count three deliberate cycles, with a few seconds between transitions, and expect six alternating contact reports. Initial/cached or battery publications are separate from counted actions. Zero LQI alone is not a failed delivery: our [investigation](LQI_RSSI_Investigation.md) traced zeros to the SDK while contact reports still arrived. Battery updates may arrive later; cached battery values are not proof of fresh battery RF traffic.

**8. Shut down cleanly.**

For the foreground setup above, press **Ctrl+C in Zigbee2MQTT first** and wait for its stopped/port-closed messages. Then stop your foreground MQTT watcher/broker with Ctrl+C if desired. These foreground commands create no service or boot-time startup. Service/container installations instead use their own normal stop/start controls. On our preconfigured Windows lab use the shortcuts below; its launcher owns the process arrangement. Exit Python utilities before starting the host again.

### First-run checkpoints

| Stage | Expected result |
|---|---|
| Arduino Verify/Upload | Successful compile/upload; blue waiting state after boot |
| Read-only Python probe | 4/4 PASS |
| Host network startup | Zigbee2MQTT started, frontend reachable, green idle |
| Device commissioning | Successful interview; no further re-pairing for routine use |
| Three door cycles | Six alternating contact states; white activity flashes |
| Routine host restart | Existing network/device resumes without recommissioning |

Keep your own backup before performing optional power-cycle/reliability trials. The broader [acceptance plan](GO_FORWARD.md) defines checks beyond this initial smoke test.

## Start the installed Windows lab

See [installed software, paths, services, startup/shutdown and VM handoff](INSTALLED_SOFTWARE.md) for the complete operating record. No Windows service or automatic login startup was created.

Double-click **`Start-Lab.cmd`**, then open **http://127.0.0.1:8080**. The installed lab uses Mosquitto 2.1.2 on `127.0.0.1:1883`, Zigbee2MQTT 2.14.2, and the ESP32 on COM17. The contact sensor is named **UZ-6D_Door**. In its displayed state, `contact: true` means closed; `false` means open.

Double-click **`Stop-Lab.cmd`** before running any Python serial command or flashing. It requests Zigbee2MQTT's normal shutdown, saves its database, and stops the broker if the launcher started it. Double-click **`Watch-Contact.cmd`** for a live MQTT contact feed; Ctrl+C stops just the watcher. The lab starts on demand, rather than automatically at Windows login.

These shortcuts require the local installation prepared in this workspace. Generated configuration is in `lab-zigbee2mqtt`; Zigbee2MQTT is in `build/zigbee2mqtt`; Mosquitto is in `%LOCALAPPDATA%/ESP32-ZNP-Lab/mosquitto`. Keep the private lab data when updating firmware.

## Test the board with Python

From this project folder in Windows PowerShell:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe tools\vm_menu.py
```

Close Zigbee2MQTT before opening the utility: COM17 has one owner. Choose the board by port number when prompted.

| Menu | Action and expected result |
|---|---|
| 1 | List ports; select COM17 on this Windows host |
| 2 | Four read-only ZNP checks; product 1, revision 20240710 |
| 5 | Read status; healthy radio diagnostic `0x00000000` |
| 6 | Resume the saved network after reset; state 9, green LED |
| 7 | Open joining for 60 seconds; cyan LED; put a new sensor in pairing mode |
| 8 | Close joining; green LED |
| 9 | Watch received traffic for 60 seconds; operate the enrolled contact sensor |

Exact commands, with the virtual environment active:

```powershell
python tools/znp_vm_probe.py --port COM17 --jsonl znp-probe.jsonl
python tools/znp_lab.py --port COM17 status
python tools/znp_lab.py --port COM17 resume
python tools/znp_lab.py --port COM17 join --seconds 60 --watch 60
python tools/znp_lab.py --port COM17 join --seconds 0 --watch 0
python tools/znp_lab.py --port COM17 --jsonl contact-events.jsonl watch --seconds 60
```

Menu 6 needs network parameters previously configured by the host. Menu 7 allows joining; the Python utility does not perform a complete device interview or configure reporting. The tested UZ-6D was already interviewed and IAS-enrolled using unmodified herdsman. Operate its magnet while menu 9 is watching. Received IAS messages use cluster `0x0500`; keep the transcript as RF evidence.

## Standard RGB colors

| Color | Meaning |
|---|---|
| Steady green | Coordinator ready, including idle |
| White flash, 120 ms | Valid received Zigbee application data, including door/window reports |
| Steady cyan | Pairing/joining is open |
| Alternating dim/bright blue | Serial ready, waiting for host network startup |
| Blinking purple | Initializing or forming the network |
| Brief amber | Rejected command, bad checksum, or queue overflow |
| Blinking red | Radio or persistent-storage fault; run menu 5 |

White returns to the previous green/cyan state. Battery messages and other application data also flash white; USB polling does not. Warning/fault indications have priority. White does not encode the contact's open/closed state. Firmware implementation: [StatusLed.h](../Arduino/TI_ZNP_ESP32_RealBridge/StatusLed.h), fed by the real APS receive queue, rather than serial-byte activity.

## What has been established

The [go-forward test plan](GO_FORWARD.md) lists the next reliability/compatibility checks, their pass criteria and the unfinished security backup/restore gate. Start with repeated host restarts and board power cycles.

On the WeAct ESP32-C6FH4, real hardware tests with unmodified zigbee-herdsman 11.1.0 passed adapter and controller startup, loaded the coordinator endpoints, paired/interviewed a UZ-6D identifying as Wing TS0203, enrolled IAS, and received both contact states. The full, unmodified Zigbee2MQTT 2.14.2 application (its bundled herdsman 10.10.0), connected to Mosquitto 2.1.2, also resumed after reflash and published both contact states and battery reports. See [RF acceptance](RF_ACCEPTANCE.md) for the limits of that result.

Passing the Python probe does **not** establish TI Zigbee 3.0 equivalence, certification, full ZNP coverage, or safe backup migration. The advertised TI identity is deliberate serial emulation. Host driver startup and real sensor traffic provide compatibility evidence only for exercised paths. Per-device security backup/restore, additional device families, and reference-dongle trace comparison remain separate work.

See [complete Python/Ubuntu tests](TESTING.md), [build instructions](BUILDING.md), [VirtualBox USB setup](VM_USB_ZIGBEE2MQTT.md), and [compatibility matrix](COMPATIBILITY_MATRIX.md). Keep generated lab configuration, network keys, databases, and raw logs out of the public repository.

## Safe updates and rollback

For a coordinator with paired devices, stop its host cleanly, preserve its private host configuration/database and working application image, and retain the current network state. Use an approved **application-only** update with unchanged SDK/partition/USB settings; keep NVS and Zigbee storage. Do not enable full erase, upload a merged full-flash image, generate replacement network settings or re-pair devices as a routine update step. The firmware identity remains deliberately TI-compatible; its displayed 2.7.1 is not a reason to upgrade dependencies.

Follow [the detailed backup/application-only/rollback procedure](LQI_Validation_Report.md). Its checkpoint paths identify private files on our development machine: another installation must save its **own** working image and backups first. Returning to an older application must retain current NVS/security counters; blindly restoring an old raw flash snapshot can roll counters backward. A host `coordinator_backup.json` alone does not provide our currently incomplete per-device TI security migration support.

## If a checkpoint fails

| Symptom | First check |
|---|---|
| No serial port | Data-capable cable, correct USB connector and actual Windows/VM ownership |
| Upload cannot connect | Selected board/port and BOOT/RESET sequence; stop every serial owner |
| Probe times out or port busy | Close Z2M/Serial Monitor/other utilities; check native USB CDC and port selection |
| Blue after upload | Expected until a host configures/starts the Zigbee network |
| MQTT connection refused | Start/check the broker before Zigbee2MQTT; verify endpoint/credentials |
| Frontend unavailable | Check host startup logs, chosen frontend port and which machine owns localhost |
| Device does not join | Host running, Permit join open, manufacturer's pairing procedure; preserve existing networks |
| Blinking red | Stop guessing; read the exact status code using Python menu 5 after the host releases serial |
| Brief amber | Inspect rejected-command/checksum/drop evidence; the intentional checksum-boundary diagnostic can cause a brief warning |
| Reports arrive with LQI 0 | Record actual event delivery and raw metadata if investigating; do not force a replacement quality value |

Raw logs can contain network keys or device payloads. Keep them private and redact them before sharing an issue. Firmware code and tested results are beta; passing a probe or six-event test is not full TI/Zigbee certification.
