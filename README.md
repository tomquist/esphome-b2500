
# ESPHome B2500 Config Generator

## Overview

This project is a React-based application that generates ESPHome configuration files for B2500 devices. It provides an intuitive form for users to fill out, which then creates a YAML configuration file. Users can download this configuration file or trigger a build process that generates a firmware image, which can be downloaded and uploaded to an ESP32 device.

## Features

- Generate ESPHome configuration files using a user-friendly form.
- Download the generated YAML configuration file.
- Trigger a build process to create a firmware image without having to install ESPHome.

## Usage

### Creating a Configuration File

1. Visit https://tomquist.github.io/esphome-b2500/
2. Fill out the form with your desired configuration settings. The default values are provided for convenience.
3. Once you have filled out the form, the configuration will be automatically generated and displayed.
4. You can download the YAML configuration file by clicking the "Download YAML" button.

### Triggering a Build

1. Click the "Build Firmware" button.
2. A modal will appear asking for a build identifier and a password.
   - The identifier is a unique name for your build, which helps in tracking it.
   - A password will be auto-generated for you, ensuring it meets the required length of at least 8 characters.
3. Optionally, you can change the auto-generated password.
4. Click the "Submit" button to trigger the build process. The submit button will display a loading indicator while the build is in progress.
5. A new tab will open, directing you to the GitHub Actions page where you can track the build progress.

### Downloading the Firmware Image

1. Once the build process is complete, go to the GitHub Actions page: [GitHub Actions](https://github.com/tomquist/esphome-b2500/actions/workflows/build-esphome.yml).
2. Find the build corresponding to your identifier.
3. Download the generated firmware image (ZIP file).

### Uploading the Firmware to an ESP32 using ESPHome Web Installer

1. Extract the firmware binary from the downloaded ZIP file.
2. Go to the ESPHome Web Installer: [ESPHome Web Installer](https://web.esphome.io/).
3. Click the "Install" button.
4. Select "Choose File" and upload the extracted firmware binary.
5. Follow the on-screen instructions to select the connected ESP32 device and flash the firmware.

## MQTT Topics

The base topic prefix is `b2500`, unless you changed it via "MQTT > Topic". Replace `{storage}` with `1`, `2` or `3` for the respective device, and `{timer}` with a number from `1` to `5` for timer settings.

### System-wide Topics

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Total System Input Power | b2500/S/pv/power | - | v1, v2 |
| Total System Output Power | b2500/S/power/power | - | v1, v2 |
| Total System Daily Energy Input | b2500/S/pv/energy | - | v1, v2 |
| Total System Daily Energy Output | b2500/S/power/energy | - | v1, v2 |
| Controller Restart | - | b2500/restart/set | v1, v2 |

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Debug Logs | b2500/debug | - | v1, v2 |
| ESP32 Temperature | b2500/esp32/temperature | - | v1, v2 |
| ESP32 Uptime | b2500/esp32/uptime | - | v1, v2 |

### Device Information

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Device Generation | b2500/{storage}/device/generation | - | v1, v2 |
| Device Name | b2500/{storage}/device/name | - | v1, v2 |
| Device Type | b2500/{storage}/device/type | - | v1, v2 |
| Device ID | b2500/{storage}/device/id | - | v1, v2 |
| Device Firmware Version | b2500/{storage}/device/fw_version | - | v1, v2 |
| Device MAC Address | b2500/{storage}/device/ble_mac | - | v1, v2 |
| FC41D Firmware Version | b2500/{storage}/device/fc41d_fw | - | v1, v2 |
| Device Scene | b2500/{storage}/device/scene | - | v1, v2 |
| Device Region | b2500/{storage}/device/region | - | v1, v2 |
| Last Response | b2500/{storage}/device/last_response | - | v1, v2 |
| Device Time | b2500/{storage}/device/time | - | v2 only |

### Connection Status

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| WiFi SSID | b2500/{storage}/device/wifi_ssid | - | v1, v2 |
| WiFi Connected Status | b2500/{storage}/device/wifi_ok | - | v1, v2 |
| WiFi Configuration | - | b2500/{storage}/wifi/set | v1, v2 |
| MQTT Connected Status | b2500/{storage}/device/mqtt_ok | - | v1, v2 |
| MQTT Configuration | - | b2500/{storage}/mqtt/set | v1, v2 |
| MQTT Reset | - | b2500/{storage}/mqtt/reset | v2 only |
| Bluetooth Status | b2500/{storage}/device/ble_ok | - | v1, v2 |
| Bluetooth Enable | b2500/{storage}/bluetooth/enabled | b2500/{storage}/bluetooth/enabled/set | v1, v2 |

### Power

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| PV Input 1 Power | b2500/{storage}/pv1/power | - | v1, v2 |
| PV Input 2 Power | b2500/{storage}/pv2/power | - | v1, v2 |
| Total Input Power | b2500/{storage}/pv/power | - | v1, v2 |
| Total System Input Power | b2500/S/pv/power | - | v1, v2 |
| Output 1 Power | b2500/{storage}/power1/power | - | v1, v2 |
| Output 2 Power | b2500/{storage}/power2/power | - | v1, v2 |
| Total Output Power | b2500/{storage}/power/power | - | v1, v2 |
| Total System Output Power | b2500/S/power/power | - | v1, v2 |
| Daily Energy Input | b2500/{storage}/pv/energy | - | v1, v2 |
| Daily Energy Output | b2500/{storage}/power/energy | - | v1, v2 |
| Total System Daily Energy Input | b2500/S/pv/energy | - | v1, v2 |
| Total System Daily Energy Output | b2500/S/power/energy | - | v1, v2 |

## Status

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| PV Input 1 Active | b2500/{storage}/pv1/active | - | v1, v2 |
| PV Input 2 Active | b2500/{storage}/pv2/active | - | v1, v2 |
| PV Input 1 Transparent | b2500/{storage}/pv1/transparent | - | v1, v2 |
| PV Input 2 Transparent | b2500/{storage}/pv2/transparent | - | v1, v2 |
| Output 1 Active | b2500/{storage}/power1/active | - | v1, v2 |
| Output 2 Active | b2500/{storage}/power2/active | - | v1, v2 |
| Output 1 Enabled | b2500/{storage}/power1/enabled | b2500/{storage}/power1/enabled/set | v1 only |
| Output 2 Enabled | b2500/{storage}/power2/enabled | b2500/{storage}/power2/enabled/set | v1 only |
| Extension 1 Connected | b2500/{storage}/extern1/connected | - | v1, v2 |
| Extension 2 Connected | b2500/{storage}/extern2/connected | - | v1, v2 |
| Temperature 1 | b2500/{storage}/device/temp1 | - | v1, v2 |
| Temperature 2 | b2500/{storage}/device/temp2 | - | v1, v2 |

### Battery

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Battery Level (SOC) | b2500/{storage}/battery/remaining_percent | - | v1, v2 |
| Battery Capacity | b2500/{storage}/battery/remaining_capacity | - | v1, v2 |
| Cell Voltage Data | b2500/{storage}/battery/cell_voltage | - | v1, v2 |
| Charge Mode | b2500/{storage}/battery/charge_mode | b2500/{storage}/battery/charge_mode/set | v1, v2 |
| Discharge Threshold | b2500/{storage}/battery/discharge_threshold | b2500/{storage}/battery/discharge_threshold/set | v1 only |
| Depth of Discharge | b2500/{storage}/battery/dod | b2500/{storage}/battery/dod/set | v1, v2 |

### Smart Meter (v2 only)

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Smart Meter Connected | b2500/{storage}/smartmeter/connected | - | v2 only |
| Smart Meter Enabled | b2500/{storage}/smartmeter/enabled | b2500/{storage}/smartmeter/enabled/set | v2 only |
| Smart Meter Power Out | b2500/{storage}/smartmeter/out | - | v2 only |
| Smart Meter Value | b2500/{storage}/smartmeter/value | - | v2 only |

### Timer Settings (v2 only)

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Timer Enabled | b2500/{storage}/timer/{timer}/enabled | b2500/{storage}/timer/{timer}/enabled/set | v2 only |
| Timer Start Time | b2500/{storage}/timer/{timer}/start | b2500/{storage}/timer/{timer}/start/set | v2 only |
| Timer End Time | b2500/{storage}/timer/{timer}/end | b2500/{storage}/timer/{timer}/end/set | v2 only |
| Timer Output Power | b2500/{storage}/timer/{timer}/power | b2500/{storage}/timer/{timer}/power/set | v2 only |

### System Control

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Device Reboot | - | b2500/{storage}/reboot/set | v1, v2 |
| Factory Reset | - | b2500/{storage}/factory_settings/set | v1, v2 |

### Value Formats

- Cell Voltage Data (`b2500/{storage}/battery/cell_voltage`):
```json
{
    "cells": [3.325, 3.324, 3.324, 3.324, 3.324, 3.325, 3.325, 3.324, 3.323, 3.324, 3.324, 3.323, 3.325, 3.323],
    "min": 3.323,
    "max": 3.325,
    "avg": 3.324071429,
    "sum": 46.537,
    "delta": 0.002
}
```

## Acknowledgments

- Thanks to [@noone2k](https://github.com/noone2k) for reverse engineeing the bluetooth protocol and creating the [initial ESPHome config](https://github.com/noone2k/hm2500pub)
- Thanks to the [ESPHome](https://esphome.io/) community for their excellent documentation and tools.
- Thanks to [Material-UI](https://mui.com/) for providing the UI components used in this project.
