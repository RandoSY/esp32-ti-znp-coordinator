# Getting started: ESP32 ZNP lab

The ESP32 presents a TI Z-Stack 3.x ZNP serial personality to the unmodified host `zstack` driver. Its own native 802.15.4 radio runs the Zigbee network. It does not execute TI firmware or reproduce a CC2652P board's amplifier/range.

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
