# Low-cost Tuya Zigbee door/window test sensor

![Owner-supplied retailer image of the Tuya Zigbee door/window sensor](assets/tuya-zigbee-door-window-sensor.png)

This is the low-cost physical sensor used to test the WeAct ESP32-C6 TI ZNP coordinator emulator. The project owner identified the unit as **UZ-6D** and supplied the product image and purchasing reference on **2026-10-18**. Its saved Zigbee interview independently reports **manufacturer `Wing`, model `TS0203`, type `EndDevice`, power source `Battery`**. The listing describes a Tuya Zigbee 3.0 smart door/window sensor; listing branding and on-network identifiers are recorded separately.

## Price and supplier reference

The project owner reported an offer **below US$4** through AliExpress Bundle Deals. This is an owner-reported offer at the reference date, not a guaranteed current price or independently verified final checkout total. Promotions, variants, delivery and taxes can change the total.

- [AliExpress bundle reference supplied by the project owner](https://www.aliexpress.com/ssr/300000512/BundleDeals2?productIds=1005012531959731%3A12000059134993720)
- Product ID: `1005012531959731`
- Variant/SKU reference: `12000059134993720`

The link retains the product/variant reference and omits the original advertising/session tracking parameters. The listing could not be independently retrieved during this documentation update. Compatibility evidence below belongs to the tested physical unit; a similar image or Tuya/TS0203 label does not establish identical firmware or hardware.

## What was tested

The observed path was:

```text
UZ-6D / Wing TS0203 sensor
    -> real Zigbee RF -> WeAct ESP32-C6 native radio/stack
    -> TI ZNP serial interface -> unmodified Zigbee2MQTT zstack driver
    -> Mosquitto MQTT -> contact/battery state
```

See the [working Zigbee2MQTT demonstration](ZIGBEE2MQTT_DEMO.md) for the actual host screenshot, four matching MQTT publications and an explanation of the TI serial emulation between the sensor's real RF traffic and the unmodified `zstack` driver.

| Item | Observed result |
|---|---|
| Coordinator | WeAct Studio ESP32-C6-MINI, 4 MB, native USB, Windows COM17 |
| Full application | Zigbee2MQTT 2.14.2, bundled zigbee-herdsman 10.10.0, Mosquitto 2.1.2 |
| Pairing and interview | PASS; IAS Zone enrollment completed |
| Contact reports | PASS; `contact: false` = open, `contact: true` = closed |
| Battery-related state | Battery and related attributes received/published in the earlier acceptance run; fresh battery RF delivery was not revalidated in the final checksum-correction smoke test |
| Existing network retention | PASS across the exercised app-only updates/host resumes, without re-pairing |
| Same-room LQI capture | 22 accepted IAS indications; raw LQI 81..209 |
| Bathroom LQI capture | 21 indications; raw histogram 0:11, 5:4, 10:2, 15:4; contact reports continued |
| Return-local LQI capture | 20 indications; raw LQI 173..209, no zeros |
| LQI path correlation | All 63 raw SDK values equalled bridge/encoded ZNP values and the ordered MQTT observations |
| Final corrected-build sensor check | Three operator-counted cycles; exactly six reports, `false,true,false,true,false,true`, LQI 163..188 |
| RGB policy | Green idle; brief white received-data flash. Operator confirmed this during the earlier diagnostic test; LED code was unchanged by the later checksum correction |

The raw-zero values originated in Espressif APS metadata; no arbitrary minimum, RSSI conversion or smoothing was introduced. These tests establish useful real-device compatibility for this unit and host version. They do not establish calibrated signal measurements, all-device support, long-term battery life, a physical loss/duplicate rate, or full TI/Zigbee certification.

The retail image includes phone/application marketing. The measured project path is the local Zigbee/ZNP/Zigbee2MQTT/MQTT chain above; phone-alert services were not part of these tests.

## Reproduce a basic sensor check

Follow [Getting Started](START_HERE.md) for Arduino IDE setup and host commissioning. Open Permit join only for a new device, follow that unit's pairing instructions, and wait for interview/enrollment to finish. The already paired test sensor does not need resetting or re-pairing for routine tests.

With Zigbee2MQTT running, operate the magnet through three deliberate open/close cycles with a few seconds between transitions. Count six physical transitions and compare with six alternating reports; keep a timestamped action log for loss/delay analysis. White flashes identify received application traffic rather than a particular contact state. The development friendly name is `UZ-6D_Door`, giving MQTT topic `esp32_znp_lab/UZ-6D_Door`; another installation may use a different name/topic.

Use the MQTT watcher while the host owns serial. For the Python serial menu, stop the host cleanly first, use the existing saved network, then restart the host afterward. See [Python tests and RGB meanings](TESTING.md), [RF acceptance](RF_ACCEPTANCE.md), [LQI investigation](LQI_RSSI_Investigation.md), and [checksum-correction hardware evidence](ZNP_Checksum_Boundary.md).

The image was supplied by the project owner; third-party image and branding rights remain with their owners. See [NOTICE](../NOTICE.md).
