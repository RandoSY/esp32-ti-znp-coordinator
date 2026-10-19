# Installed Windows lab: locations, startup and shutdown

Recorded 2026-10-18. Project folder:

```text
C:\Users\rasyoung\Desktop\TI_ZNP_ESP32_VM_Bridge_v0.2.1
```

## What is installed

| Component | Version / location | How it runs |
|---|---|---|
| Eclipse Mosquitto broker and command-line clients | 2.1.2, `C:\Users\rasyoung\AppData\Local\ESP32-ZNP-Lab\mosquitto` | Normal background process, started by the lab launcher |
| Zigbee2MQTT | 2.14.2, project `build\zigbee2mqtt`; official release commit `5c0c1c60078e1737c5dee5f8217583bca7610e66` | Normal background Node.js process through `tools\lab_host.cjs` |
| Zigbee2MQTT's bundled herdsman | 10.10.0, inside that installation's `node_modules` | Loaded by Zigbee2MQTT; do not start separately |
| Zigbee2MQTT web interface | Bundled with Zigbee2MQTT | Served by that process at http://127.0.0.1:8080 |
| Independent herdsman developer test dependency | 11.1.0, project `build\host\node_modules` | Only developer test scripts use it; unnecessary for normal operation |
| Dependency tooling | pnpm 11.26.0 selected by the Zigbee2MQTT package; downloaded into the user's pnpm/npm caches | Only needed to install/build/update dependencies |

Mosquitto's official Windows package was extracted into a user-owned folder. Its broker, clients and required DLLs run there; the installer was not used to register a service. The download/source inspection files remain in project `build\mosquitto`. Zigbee2MQTT was cloned from the official release, installed with its frozen lockfile and compiled without source/driver patches.

Existing tools used include Node.js **22.20.0** (`C:\Program Files\nodejs`), npm, Python/pyserial, Git, 7-Zip, Arduino CLI and Arduino-ESP32 **3.3.11**. These are development prerequisites, not separate programs to start for the lab.

Official sources: [Mosquitto downloads](https://mosquitto.org/download/), [Zigbee2MQTT 2.14.2](https://github.com/Koenkk/zigbee2mqtt/releases/tag/2.14.2), [Windows installation guide](https://www.zigbee2mqtt.io/guide/installation/05_windows.html).

## Services and automatic startup

**No Windows Mosquitto or Zigbee2MQTT service was created.** The Windows service inventory was checked. Neither program was configured to start at Windows login or boot. Closing/restarting Windows ends this lab; start it again with the shortcut when needed.

You do not need Services Manager, administrator elevation, `net start`, a separate MQTT broker window, Arduino IDE, Serial Monitor, or the developer test scripts for ordinary operation. The launcher starts the broker and application together. Its background processes continue after the start command's window closes.

## Everyday use

From the project folder:

1. Connect the ESP32 to Windows and confirm COM17 belongs to Windows, rather than the VM.
2. Double-click **`Start-Lab.cmd`**. It reports readiness or the log to inspect. Running it again detects the existing managed process.
3. Open **http://127.0.0.1:8080**. The existing sensor is **UZ-6D_Door**.
4. Optionally double-click **`Watch-Contact.cmd`** for a live MQTT feed. `contact: true` means closed; `false` means open. Ctrl+C ends only the watcher.
5. Double-click **`Stop-Lab.cmd`** when finished, before flashing, before Python serial utilities, or before VM USB capture.

The stop command requests Zigbee2MQTT's normal shutdown, allowing database/backup writes and serial close. It then stops Mosquitto if the launcher started it. If an independent broker was already listening when the launcher started, that broker is reused and is not stopped by this launcher.

After `Stop-Lab.cmd` says it stopped cleanly, the ESP32 may remain green: its radio stays running while powered. This is expected and does not mean Windows still owns the serial port. A board reset returns it to blue until a host resumes the network. Do not unplug the board just to release COM17.

## Configuration and files to retain

| Path relative to the project | Purpose |
|---|---|
| `lab-zigbee2mqtt\configuration.yaml` | Active COM17, MQTT, network, frontend and sensor settings; contains the private Zigbee key |
| `lab-zigbee2mqtt\mosquitto.conf` | Broker configuration: `127.0.0.1:1883`, local anonymous access, persistence disabled |
| `lab-zigbee2mqtt\database.db` | Host device interview, endpoints and configuration state |
| `lab-zigbee2mqtt\state.json` | Cached device states; replay after restart is not evidence of a fresh radio report |
| `lab-zigbee2mqtt\coordinator_backup.json` | Generated network backup; **per-device TI security backup/restore remains incomplete** |
| `lab-zigbee2mqtt\log\...\log.log` | Zigbee2MQTT application logs |
| `lab-zigbee2mqtt\lab-process.json` | Current launcher process IDs, not a service definition |
| `artifacts\lab-host.stdout.log`, `lab-host.stderr.log` | Launcher/application/broker output for the most recent start |
| `artifacts\esp32c6\lab-network.json` | Original private matching network options used by test/config tools |
| `artifacts\esp32c6\mqtt-contact-evidence.log` | Contact/battery MQTT messages captured during the live acceptance test |

Keep the lab data and board NVS together. An ordinary firmware app update preserves NVS; a full erase destroys the saved radio network. Do not delete the installed `build\zigbee2mqtt` folder while using these shortcuts. `build/`, `artifacts/`, `lab-zigbee2mqtt/`, logs and archives are excluded from GitHub by `.gitignore`.

The MQTT endpoint and web interface are bound to Windows loopback. The guest must use its independently configured MQTT/application installation; `localhost` inside Ubuntu refers to Ubuntu. This session did not install or configure Ubuntu services. The Ubuntu installation is being handled separately.

## Switching the board between Windows and Ubuntu VM

Always shut down the application using the board before transferring USB ownership. For Windows to Ubuntu: run **`Stop-Lab.cmd`**, close Python/Arduino serial programs, then capture the ESP32 through VirtualBox **Devices -> USB**. Windows COM17 is unavailable while Ubuntu owns the device; use the guest's detected tty path there.

Before returning to Windows, stop the guest's Zigbee host/serial utility, release the USB device from the VM, confirm COM17 returns, then run **`Start-Lab.cmd`**. Do not run both hosts against the same physical serial connection. Keep both host configurations aligned with the board's saved network; do not let a second host silently choose a different PAN/key.

## Troubleshooting

- **COM17 busy/missing:** stop the current host/utility and check VM USB ownership.
- **Frontend unavailable:** rerun Start-Lab and read its error/log paths. Do not start a second broker or application manually.
- **Shutdown still pending:** inspect logs and wait for database/serial shutdown; the stop shortcut does not force-kill a stuck process.
- **USB reconnect changes the COM number:** update `serial.port` in the private active configuration while stopped.
- **Want Python tests:** stop the lab, follow [Getting Started](START_HERE.md), then start the lab again afterward.

Verified here: startup, repeated-start detection, graceful stop, COM17 release, Python probe 4/4 and diagnostic zero, full application restart, and live contact/battery MQTT messages. That acceptance covers the exercised Windows sensor path; full TI equivalence and migration restoration remain separate work.
