# Ubuntu 26.04.1: native Home Assistant and Zigbee host

Prepared 2026-10-19 for an Intel Celeron N4000-class, 6 GB RAM, 512 GB SSD server. These are operator-run instructions, not a record of an installation performed on that server. Ubuntu and the existing coordinator firmware are retained. No Docker or virtual machine is used.

Home Assistant Core in a Python virtual environment remains technically possible, but this **installation method has been unsupported by Home Assistant since 2025.12**. Updates, Python dependencies and the system service are the operator's responsibility. There is no Supervisor or app/add-on store. Native Mosquitto and Zigbee2MQTT supply the other programs separately. See the [official installation-method notice](https://www.home-assistant.io/blog/2025/05/22/deprecating-core-and-supervised-installation-methods-and-32-bit-systems/).

Use one section at a time and stop if a command fails. Commands labelled Bash run on Ubuntu; the export section explicitly uses Windows PowerShell. Existing users, directories, services or occupied ports mean an existing installation needs inspection, rather than running the corresponding fresh-install section over it.

## 1. Why a detected dongle might not be green

`dmesg` reporting `ttyACM0` establishes USB enumeration. It does not install or start the host application. The firmware normally waits for TI ZNP initialization from Zigbee2MQTT before its radio reaches the green online state.

The chain is:

```text
Zigbee devices <-> ESP32-C6 <-> Zigbee2MQTT <-> Mosquitto <-> Home Assistant
```

**Zigbee2MQTT and its MQTT broker are needed for the normal host startup. Home Assistant is optional for that startup.** USB attachment does not automatically create a Linux service. The LED's online state does not independently prove MQTT or Home Assistant health.

Read-only checks:

```bash
systemctl is-active zigbee2mqtt mosquitto
ls -l /dev/serial/by-id/
ls -l /dev/ttyACM0
sudo fuser -v /dev/ttyACM0
```

An `inactive`, `unknown` or `not-found` result can mean that no service was installed under that name. A manually running application may still own the port; `fuser` helps identify it. No output from `fuser` normally means no process currently has it open.

LED reference: blue = waiting/initializing, purple = commissioning initialization, cyan = joining permitted, green = online idle, brief white = application reception, amber = protocol/drop warning, flashing red = radio or persistence fault. A red fault needs investigation; it is not fixed by simply declaring the host installed.

**Do not reset, erase, reflash or re-pair as an installation step.** Keep the same physical coordinator and its current persistent state. Copy its matching, complete Zigbee2MQTT data folder before starting a different host. Do not use a fresh `GENERATE` network configuration with this paired coordinator.

## 2. Check Ubuntu and existing programs

Use Ethernet. Reserve the server's address in the router's DHCP settings so the Windows browser has a stable destination.

```bash
cat /etc/os-release
uname -m
free -h
df -h /
hostname -I
python3 --version
systemctl list-unit-files 'homeassistant*' 'zigbee2mqtt*' 'mosquitto*'
sudo ss -ltnp '( sport = :8123 or sport = :1883 or sport = :8080 )'
```

Expected architecture is `x86_64`. Record the Ubuntu release and server IP. Do not install duplicate services against existing listeners. This guide uses names `homeassistant`, `mosquitto` and `zigbee2mqtt`; inspect any existing installation first.

The pinned Home Assistant package below is **2026.10.2**, whose [release metadata](https://github.com/home-assistant/core/blob/2026.10.2/pyproject.toml) requires **Python >=3.14.2**. Ubuntu 26.04 publishes [Python 3.14 packages](https://packages.ubuntu.com/resolute/python3.14). The explicit version check below prevents accidentally using an older interpreter.

## 3. Install Home Assistant's prerequisites

```bash
sudo apt update
sudo apt install python3.14 python3.14-venv python3.14-dev \
  build-essential autoconf pkg-config libssl-dev libffi-dev libudev-dev \
  libxml2-dev libxslt1-dev libjpeg-dev zlib1g-dev \
  libavformat-dev libavcodec-dev libavdevice-dev libavutil-dev \
  libswscale-dev libswresample-dev libavfilter-dev ffmpeg libgammu-dev \
  ca-certificates curl

python3.14 -c 'import sys; assert (3,14,2) <= sys.version_info[:3] < (3,15,0), sys.version; print(sys.version)'
```

If a package is unavailable, check Ubuntu's enabled repositories and mirrors; do not substitute an old Home Assistant release or replace Ubuntu's system Python. These build-library prerequisites follow the [manual Core development guidance](https://contributors.home-assistant.io/core/development/environment/); this installation uses the published release package rather than the development branch.

## 4. Create a separate account and Python environment

Fresh-install paths below must not already contain another installation. Check first:

```bash
getent passwd homeassistant
ls -ld /opt/homeassistant-2026.10.2 /opt/homeassistant-current \
  /var/lib/homeassistant /etc/systemd/system/homeassistant.service 2>/dev/null
```

For this fresh install, an absent account and absent paths are expected. If they exist, stop here and inspect them; do not overwrite them.

```bash
sudo useradd --system --user-group --create-home \
  --home-dir /var/lib/homeassistant --shell /usr/sbin/nologin homeassistant

sudo install -d -m 0700 -o homeassistant -g homeassistant \
  /var/lib/homeassistant/config
sudo chmod 0750 /var/lib/homeassistant
sudo install -d -o homeassistant -g homeassistant /opt/homeassistant-2026.10.2

sudo -H -u homeassistant python3.14 -m venv /opt/homeassistant-2026.10.2
sudo -H -u homeassistant /opt/homeassistant-2026.10.2/bin/python \
  -m pip install --upgrade pip
sudo -H -u homeassistant /opt/homeassistant-2026.10.2/bin/python \
  -m pip install --prefer-binary 'homeassistant==2026.10.2'
sudo -H -u homeassistant /opt/homeassistant-2026.10.2/bin/python -m pip check

sudo ln -s /opt/homeassistant-2026.10.2 /opt/homeassistant-current
```

This Python environment isolates application packages; it is not a virtual machine. Do not use `sudo pip install` against Ubuntu's system interpreter. The first installation and first startup may take several minutes while dependencies are downloaded.

## 5. Configure Home Assistant and its system service

Create a new configuration file:

```bash
sudo -H -u homeassistant tee /var/lib/homeassistant/config/configuration.yaml >/dev/null <<'YAML'
default_config:

homeassistant:
  time_zone: America/Indiana/Indianapolis
YAML
```

Create the service:

```bash
sudo tee /etc/systemd/system/homeassistant.service >/dev/null <<'UNIT'
[Unit]
Description=Home Assistant Core (native Python environment)
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
User=homeassistant
Group=homeassistant
WorkingDirectory=/var/lib/homeassistant
Environment=HOME=/var/lib/homeassistant
Environment=TZ=America/Indiana/Indianapolis
Environment=PATH=/opt/homeassistant-current/bin:/usr/local/bin:/usr/bin:/bin
ExecStart=/opt/homeassistant-current/bin/hass -c /var/lib/homeassistant/config
Restart=on-failure
RestartSec=10
TimeoutStopSec=120
UMask=0077

[Install]
WantedBy=multi-user.target
UNIT

sudo systemd-analyze verify /etc/systemd/system/homeassistant.service
sudo systemctl daemon-reload
sudo systemctl enable --now homeassistant.service
systemctl is-enabled homeassistant.service
systemctl is-active homeassistant.service
sudo journalctl -u homeassistant.service -n 60 --no-pager
```

Logs can contain personal information. Inspect locally and redact before sharing. If it repeatedly fails, stop it with `sudo systemctl stop homeassistant` and inspect the first error rather than repeatedly reinstalling.

Local HTTP check:

```bash
curl -I http://127.0.0.1:8123/
```

An HTTP response shows a listener; complete browser onboarding to establish that the application is usable.

## 6. Open it from Windows

Use the Ubuntu address from `hostname -I`, for example:

```text
http://192.168.1.50:8123
```

Do not use Windows `localhost` for the Ubuntu server. Create the owner account, store its password, and set the location and time zone. A router DHCP reservation avoids changing the address later. The server can run without a monitor or an open SSH session.

If Ubuntu's UFW firewall is active, allow TCP 8123 from your actual home subnet. This example assumes `192.168.1.0/24`; replace it to match the LAN:

```bash
sudo ufw status
sudo ufw allow from 192.168.1.0/24 to any port 8123 proto tcp
```

This does not enable or reset UFW. If you later enable it, preserve SSH and any other required access first. Keep router port forwarding for these services disabled; internet remote access can be configured separately.

## 7. Install native Mosquitto, if no broker is already installed

Skip this fresh-broker setup when a working broker already exists. Inspect and reuse its credentials/listener instead. On a fresh Ubuntu server:

```bash
sudo apt install mosquitto mosquitto-clients
```

The Ubuntu package normally starts a service. Before adding the configuration below, confirm that `/etc/mosquitto/conf.d/home-minder.conf` and `/etc/mosquitto/home-minder.passwd` do not already exist, and that another config does not already define a port-1883 listener. Do not overwrite existing broker settings.

Create a new broker account; enter and retain its password when prompted:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/home-minder.passwd homeminder
sudo chown mosquitto:mosquitto /etc/mosquitto/home-minder.passwd
sudo chmod 0600 /etc/mosquitto/home-minder.passwd

sudo tee /etc/mosquitto/conf.d/home-minder.conf >/dev/null <<'CONF'
listener 1883 127.0.0.1
allow_anonymous false
password_file /etc/mosquitto/home-minder.passwd
CONF

sudo systemctl enable mosquitto
sudo systemctl restart mosquitto
systemctl is-active mosquitto
sudo ss -ltnp '( sport = :1883 )'
```

Expected broker listener: `127.0.0.1:1883`. Both Ubuntu applications can reach it; Windows accesses their web interfaces, not this private MQTT listener. The native package version follows Ubuntu's repository, so record it with `mosquitto -h`; do not claim it is the Windows broker version. The [Mosquitto password tool documentation](https://mosquitto.org/man/mosquitto_passwd-1.html) explains that `-c` creates/overwrites a file, which is why this command is restricted to the fresh installation.

In Home Assistant, open **Settings -> Devices & services -> Add integration -> MQTT**. Use broker `127.0.0.1`, port `1883`, user `homeminder`, and the password just created. Use the existing broker's settings instead if you skipped this installation. [MQTT integration documentation](https://www.home-assistant.io/integrations/mqtt/).

## 8. Install native Zigbee2MQTT, if it is not already installed

This can be prepared before connecting the radio. It deliberately pins **2.14.2**, the host release tested with this emulator. Existing installations need review rather than a duplicate service. Native upstream reference: [Zigbee2MQTT Linux installation](https://www.zigbee2mqtt.io/guide/installation/01_linux.html).

On the Ubuntu 26.04 server:

```bash
sudo apt install nodejs npm git make g++ gcc libsystemd-dev python3-yaml
/usr/bin/node --version
```

Ubuntu's [26.04 Node package](https://packages.ubuntu.com/resolute/nodejs) supplies Node 22.22.1 at the time of writing. The tested release permits Node 22.2+ in the 22 series or Node 24; do not substitute an old Node 18/20 installation. Record the actual version.

The following account and `/opt` directories must be new:

```bash
sudo useradd --system --user-group --create-home \
  --home-dir /var/lib/zigbee2mqtt --shell /usr/sbin/nologin zigbee2mqtt
sudo chmod 0750 /var/lib/zigbee2mqtt
sudo usermod -aG dialout zigbee2mqtt
sudo install -d -m 0700 -o zigbee2mqtt -g zigbee2mqtt /var/lib/zigbee2mqtt/data
sudo install -d -o zigbee2mqtt -g zigbee2mqtt \
  /opt/zigbee2mqtt-2.14.2 /opt/zigbee2mqtt-tools

sudo -H -u zigbee2mqtt git clone --depth 1 --branch 2.14.2 \
  https://github.com/Koenkk/zigbee2mqtt.git /opt/zigbee2mqtt-2.14.2
sudo -H -u zigbee2mqtt npm install --prefix /opt/zigbee2mqtt-tools pnpm@11.26.0
sudo -H -u zigbee2mqtt sh -c '
  export PATH=/opt/zigbee2mqtt-tools/node_modules/.bin:/usr/bin:/bin
  cd /opt/zigbee2mqtt-2.14.2 &&
  pnpm install --frozen-lockfile &&
  pnpm run build
'
```

Do not start it yet. Configuration and the matching host database come first.

## 9. Transfer the existing paired coordinator's data

This section is a deliberate host handoff and entails a temporary reporting outage. Perform it when ready, with no alarm depending on this experimental setup. If the board was already moved, still stop the former host before copying its data. No fresh firmware upload or pairing is involved.

On Windows, close Python serial utilities and Arduino Serial Monitor. From the project folder, run `Stop-Lab.cmd` and verify a clean stop. In PowerShell:

```powershell
$project = 'C:\Users\rasyoung\Desktop\TI_ZNP_ESP32_VM_Bridge_v0.2.1'
Set-Location -LiteralPath $project
.\Stop-Lab.cmd
if ($LASTEXITCODE -ne 0) { throw 'Stop did not complete; inspect before continuing.' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$archive = Join-Path $env:USERPROFILE "zigbee-data-$stamp.tar.gz"
tar.exe -czf $archive -C (Join-Path $project 'lab-zigbee2mqtt') .
if ($LASTEXITCODE -ne 0) { throw 'Archive failed.' }
```

Retain this private archive as the pre-handoff host backup. Transfer it over SSH/SCP or a private removable drive. Example, replacing user/address:

```powershell
scp $archive ubuntuuser@192.168.1.50:zigbee-data.tar.gz
```

SCP requires an SSH server on Ubuntu. If needed, install `openssh-server` there and allow SSH through the existing LAN firewall policy. The archive contains the network key and other private application state; never post it to GitHub or chat.

On Ubuntu, use this extraction only for the new, empty `/var/lib/zigbee2mqtt/data` directory:

```bash
sudo ls -A /var/lib/zigbee2mqtt/data
# The command above must show no files before the following extraction.
sudo tar -xzf "$HOME/zigbee-data.tar.gz" -C /var/lib/zigbee2mqtt/data
sudo chown -R zigbee2mqtt:zigbee2mqtt /var/lib/zigbee2mqtt/data
```

The complete folder includes `configuration.yaml`, `database.db`, `state.json`, any external device/group/secret files, and `coordinator_backup.json` when present. Extra Windows launcher files are not used by native Zigbee2MQTT. Keep the original Windows copy intact.

Keep using the **same physical ESP32-C6 with its current NVS and radio storage**. Host backup files are not a complete replacement for those stores, and this emulator's per-device TI security-table restoration is incomplete. This procedure moves the host; it is not a migration to another radio or a restore of old frame counters. [Upstream host-move guidance](https://www.zigbee2mqtt.io/guide/faq/#how-do-i-move-my-zigbee2mqtt-instance-to-a-different-environment).

Find its stable USB path:

```bash
ls -l /dev/serial/by-id/
sudo fuser -v /dev/ttyACM0
```

Do not open it concurrently with another host or Python probe. Prefer the real `/dev/serial/by-id/...` path over a changing `ttyACM0` number. If no by-id link exists, inspect `/dev/serial/by-path/` for a port-specific stable path; do not invent a USB serial identifier.

Edit the **copied** configuration privately:

```bash
sudo nano /var/lib/zigbee2mqtt/data/configuration.yaml
```

Change only host connectivity and integration settings. Merge into the existing sections; do not replace the whole file or create duplicate YAML keys:

```yaml
mqtt:
  # Keep the existing base_topic and other required options.
  server: mqtt://127.0.0.1:1883
  user: homeminder
  password: 'YOUR_MQTT_PASSWORD'
serial:
  port: /dev/serial/by-id/YOUR_ACTUAL_USB_DEVICE
  adapter: zstack
  baudrate: 115200
  rtscts: false
frontend:
  enabled: true
  host: 127.0.0.1
  port: 8080
homeassistant:
  enabled: true
```

Keep the original network key, PAN ID, extended PAN ID, channel, devices, endpoint settings and application topics unchanged. Preserve any additional configuration sections and required secret references. No `GENERATE` values should replace the resolved identity of the working network. Confirm the actual USB path and that the `zigbee2mqtt` account can read and write it:

```bash
sudo -u zigbee2mqtt test -r /dev/serial/by-id/YOUR_ACTUAL_USB_DEVICE
sudo -u zigbee2mqtt test -w /dev/serial/by-id/YOUR_ACTUAL_USB_DEVICE
```

If these fail, inspect the real tty owner/group and the `dialout` membership. Do not make the device world-writable or give Home Assistant direct access to the coordinator.

Check the copied configuration and database without printing network secrets:

```bash
sudo -u zigbee2mqtt python3 - <<'PY'
from pathlib import Path
import yaml
data = Path('/var/lib/zigbee2mqtt/data')
config = yaml.safe_load((data / 'configuration.yaml').read_text())
assert isinstance(config, dict), 'Invalid configuration'
advanced = config.get('advanced', {})
for name in ('network_key', 'pan_id', 'ext_pan_id', 'channel'):
    value = advanced.get(name)
    assert value is not None, f'Missing explicit working-network setting: {name}'
    assert not (isinstance(value, str) and value.strip().upper() == 'GENERATE'), \
        f'Unresolved working-network setting: {name}'
serial = config.get('serial', {})
assert serial.get('adapter') == 'zstack', 'Expected the zstack driver'
port = serial.get('port', '')
assert port.startswith('/dev/') and Path(port).exists(), 'Set the actual Linux USB path'
assert (data / 'database.db').stat().st_size > 0, 'Missing host device database'
print('Host data/configuration preflight passed; no radio commands sent.')
PY
```

This catches missing files and unresolved settings. It does not independently prove that the configuration matches the radio's stored identity; retaining the known-working data and the same board remains essential.

## 10. Start the Zigbee host and check the existing sensor

Only after the data/identity/USB checks, create this new service:

```bash
sudo tee /etc/systemd/system/zigbee2mqtt.service >/dev/null <<'UNIT'
[Unit]
Description=Zigbee2MQTT native host for ESP32 TI ZNP coordinator
Wants=network-online.target mosquitto.service
After=network-online.target mosquitto.service
ConditionPathExists=/var/lib/zigbee2mqtt/data/configuration.yaml

[Service]
Type=simple
User=zigbee2mqtt
Group=zigbee2mqtt
SupplementaryGroups=dialout
WorkingDirectory=/opt/zigbee2mqtt-2.14.2
Environment=NODE_ENV=production
Environment=HOME=/var/lib/zigbee2mqtt
Environment=TZ=America/Indiana/Indianapolis
Environment=ZIGBEE2MQTT_DATA=/var/lib/zigbee2mqtt/data
ExecStart=/usr/bin/node /opt/zigbee2mqtt-2.14.2/index.js
Restart=on-failure
RestartSec=10
TimeoutStopSec=120
UMask=0077

[Install]
WantedBy=multi-user.target
UNIT

sudo systemd-analyze verify /etc/systemd/system/zigbee2mqtt.service
sudo systemctl daemon-reload
sudo systemctl start zigbee2mqtt
systemctl is-active zigbee2mqtt
sudo journalctl -u zigbee2mqtt -n 60 --no-pager
```

If the startup says it will form a new network, offers onboarding despite the copied configuration, reports identity mismatch, or cannot find the existing device, **stop** with `sudo systemctl stop zigbee2mqtt`. Diagnose the data/path/configuration before retrying. Do not accept reset/recommissioning as an automatic remedy.

Expected acceptance: host initializes its zstack adapter, resumes the existing network, connects to MQTT, and shows the previously paired UZ-6D. Green idle and white flashes should return with fresh application reports. Open and close the already-paired sensor three times, without pressing its pairing button. Check all six transitions in Zigbee2MQTT and Home Assistant; cached state or battery alone is not fresh RF proof. In Home Assistant the contact's device class normally displays Open/Closed; use those labels rather than assuming its boolean matches raw MQTT `contact`.

After a successful manual start and sensor check, enable future boot startup:

```bash
sudo systemctl enable zigbee2mqtt
```

The Zigbee2MQTT frontend stays private to Ubuntu loopback. From Windows with an SSH client, create a tunnel (replace user/address):

```powershell
ssh -N -L 18080:127.0.0.1:8080 ubuntuuser@192.168.1.50
```

While that window remains open, browse `http://127.0.0.1:18080` on Windows. Using 18080 avoids the old Windows frontend's port 8080. Closing the tunnel does not stop Zigbee2MQTT.

In Home Assistant, MQTT discovery should expose the sensor after Zigbee2MQTT connects with `homeassistant.enabled: true`. Keep the zstack host as sole USB owner. Do not add this radio to Home Assistant's ZHA integration. [Zigbee2MQTT discovery documentation](https://www.zigbee2mqtt.io/guide/usage/integrations/home_assistant.html).

## 11. What starts automatically, and everyday control

| Program | Service | Persistent files |
|---|---|---|
| Home Assistant Core | `homeassistant.service` | `/var/lib/homeassistant/config`, including hidden `.storage` |
| Mosquitto | `mosquitto.service` | `/etc/mosquitto`, native package data normally `/var/lib/mosquitto` |
| Zigbee2MQTT | `zigbee2mqtt.service` | `/var/lib/zigbee2mqtt/data` |

Each enabled service starts at Ubuntu boot without a logged-in desktop, terminal or Windows computer. Check `systemctl is-enabled homeassistant mosquitto zigbee2mqtt`. Home Assistant does not start or update the other two services.

```bash
# Status; these are read-only.
systemctl is-active homeassistant mosquitto zigbee2mqtt

# Follow logs; Ctrl+C leaves the service running.
sudo journalctl -fu homeassistant
sudo journalctl -fu zigbee2mqtt

# Stop applications cleanly, then the broker.
sudo systemctl stop homeassistant zigbee2mqtt
sudo systemctl stop mosquitto

# Start the broker, then applications.
sudo systemctl start mosquitto
sudo systemctl start zigbee2mqtt homeassistant

# Shut down the whole server cleanly when intended.
sudo systemctl poweroff
```

Stopping services does not erase application data. For Python serial tests or USB handoff, stop Zigbee2MQTT first and confirm the port is free; Mosquitto and Home Assistant can stay running. An LED that remains green while firmware is still powered does not prove a host process is still attached.

## 12. Backup, updates and rollback

Before changes, save software versions and make a coherent private backup during a planned outage. After all three applications have been installed:

```bash
sudo install -d -m 0700 /var/backups/home-minder
sudo systemctl stop homeassistant zigbee2mqtt
sudo systemctl stop mosquitto
stamp=$(date -u +%Y%m%dT%H%M%SZ)
sudo tar -czf "/var/backups/home-minder/state-$stamp.tar.gz" \
  -C / var/lib/homeassistant var/lib/zigbee2mqtt \
  etc/mosquitto var/lib/mosquitto \
  etc/systemd/system/homeassistant.service \
  etc/systemd/system/zigbee2mqtt.service
sudo chmod 0600 "/var/backups/home-minder/state-$stamp.tar.gz"
sudo systemctl start mosquitto
sudo systemctl start zigbee2mqtt homeassistant
```

Verify `tar` succeeded before treating the backup as valid. Store another encrypted/private copy away from this SSD. The archive includes credentials and device history. Record `python3.14 --version`, `/opt/homeassistant-current/bin/hass --version`, `mosquitto -h`, `/usr/bin/node --version` and the Zigbee2MQTT Git commit separately. Config/database backups do not capture every coordinator security counter or replace its persistent radio state.

Do not upgrade through a non-existent Supervisor/app store. For Home Assistant, retain the known-working version directory, create a separate environment for an explicitly chosen later version, install/check its dependencies, back up data, stop Home Assistant, then change the `homeassistant-current` link and start it. Do not blindly upgrade packages in the running environment. A rollback may require restoring the matching Home Assistant configuration/database as well as switching Python environments, because an upgrade may migrate stored data.

For a failed Linux host handoff, stop the Linux Zigbee2MQTT process before moving USB ownership. Keep the same current coordinator storage. Preserve Linux's latest host data, and reconcile/copy current compatible host data back before restarting Windows if it has changed. Do not blindly restore old raw flash or roll security counters backward. The original Windows folder is a checkpoint, not proof that it contains every later change.

Run a deliberate, supervised reboot test after backups and fresh-sensor acceptance: confirm the same network resumes, all three services return, Windows browser access returns, and fresh door transitions still arrive. Power-loss, long-run reliability, other sensor models and broader alarm logic remain separate tests. The experimental emulator and this Ubuntu deployment should be validated before relying on them for security alerts.

## Verification status for this document

- Confirmed from published metadata: Home Assistant 2026.10.2 exists and requires Python >=3.14.2; Ubuntu 26.04 provides Python 3.14 and Node 22 packages.
- Confirmed from the installed project reference: Zigbee2MQTT 2.14.2 uses pnpm 11.26.0; its native host supports the existing emulator's zstack protocol path on the tested Windows system.
- Guide checks performed locally: Bash and PowerShell syntax, YAML example parsing, local links and service-unit section structure. A section-structure check is not a systemd startup test.
- Ubuntu package installation, unit startup, broker authentication, browser onboarding, host transfer, fresh RF reports and boot persistence on the user's Celeron: **NOT RUN by this session**. The commands above provide the acceptance procedure; only observed results should be marked PASS.
