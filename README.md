
# ESPHome B2500 Config Generator

## Overview

This tool helps you create configuration files for your B2500 device and generate the necessary firmware without requiring any technical knowledge or software installation on your computer. You can do everything through your web browser.

## Hardware Recommendation

For the best experience with this project, I recommend using an ESP32-S3. You can purchase one from these links:
- [1x ESP32-S3 DevKitC-1](https://amzn.to/429OJDX)
- [3x ESP32-S3 DevKitC-1](https://amzn.to/3PwGRVv)

When using these boards, set the following options:
- Board: `esp32-s3-devkitc-1`
- Variant: `esp32s3`

## Usage

### Creating a Configuration File

1. Visit https://tomquist.github.io/esphome-b2500/
2. Fill out the form with your desired configuration settings. The default values are provided for convenience.
3. Once you have filled out the form, the configuration will be automatically generated and displayed.
4. You can download the YAML configuration file by clicking the "Download YAML" button. This configuration can directly be used to build an ESPHome image, e.g. through the Home Assistant ESPHome Addon. Alternatively you can directly build an image for your ESP32 and flash it via the ESPHome web flasher. Read the following section for detailed instructions.

### Build an Image on the Web

1. Click the "Build Image" button.
2. Click "Start Build" and follow the instructions
3. Extract the firmware binary from the downloaded ZIP file.
4. Connect your ESP32 via USB
5. Go to the ESPHome Web Installer: [ESPHome Web Installer](https://web.esphome.io/).
6. Click the "Install" button.
7. Select "Choose File" and upload the extracted firmware binary.
8. Follow the on-screen instructions to select the connected ESP32 device and flash the firmware.

## MQTT Topics

Find a list of all MQTT topics, depending on the selected configuration version:
<details>
   <summary>Legacy</summary>
   Find a detailed list of topics [here](https://github.com/noone2k/hm2500pub/wiki/ESP32-MQTT-TOPICS).
</details>
<details>
<summary>Native ESPHome component</summary>
   
  The base topic prefix is `b2500`, unless you changed it via "MQTT > Topic". Replace `{storage}` with `1`, `2` or `3` for the respective device, and `{timer}` with a number from `1` to `5` for timer settings.

### System-wide Topics

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Total System Input Power | b2500/S/pv/power | - | v1, v2 |
| Total System Output Power | b2500/S/power/power | - | v1, v2 |
| Total System Daily Energy Input | b2500/S/pv/energy | - | v1, v2 |
| Total System Daily Energy Output | b2500/S/power/energy | - | v1, v2 |
| Total System Remaining Capacity | b2500/S/battery/remaining_capacity | - | v1, v2 |
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
</details>
<details>
<summary>Minimal native ESPHome component</summary>

The base topic prefix is configurable via `mqtt.topic`, defaulting to `b2500`. All device-specific topics start with `{topic_prefix}/{storage}/`:
- Replace `{topic_prefix}` with the configured MQTT topic prefix (defaults to "b2500")
- Replace `{storage}` with device number (1, 2, etc.)
- Replace `{output}` with output number (1 or 2)
- `{grid_power_topic}`, `{limit_state_topic}`, and `{limit_cmd_topic}` are configurable for PowerZero feature
- Power meter topics are only available when the feature is enabled in configuration
- PowerZero features are only available when enabled in configuration

### System-wide Topics

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Debug Logs | {topic_prefix}/debug | - | v1, v2 |
| ESP32 Temperature | {topic_prefix}/esp32/temperature | - | v1, v2 |
| ESP32 Uptime | {topic_prefix}/esp32/uptime | - | v1, v2 |
| Total System Energy In | {topic_prefix}/S/pv/energy | - | v1, v2 |
| Total System Energy Out | {topic_prefix}/S/power/energy | - | v1, v2 |
| Controller Restart | - | {topic_prefix}/restart/set | v1, v2 |

### Device Information

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Device Info | {topic_prefix}/{storage}/device | - | v1, v2 |
| Runtime Info | {topic_prefix}/{storage}/runtime | - | v1, v2 |
| Cell Info | {topic_prefix}/{storage}/cell | - | v1, v2 |
| WiFi Info | {topic_prefix}/{storage}/wifi | - | v1, v2 |
| FC41D Info | {topic_prefix}/{storage}/fc41d | - | v1, v2 |
| Timer Info | {topic_prefix}/{storage}/timer | - | v2 only |

### Connection Status

| Description | Read Topic | Write Topic | Value Format Example | Available in version |
|------------|------------|-------------|---------------------|---------------------|
| Bluetooth Status | {topic_prefix}/{storage}/bluetooth/enabled | {topic_prefix}/{storage}/bluetooth/enabled/set | "ON" or "OFF" | v1, v2 |
| WiFi Configuration | - | {topic_prefix}/{storage}/wifi/set | `{"ssid": "network_name", "password": "wifi_password"}` | v1, v2 |
| MQTT Configuration | - | {topic_prefix}/{storage}/mqtt/set | `{"host": "mqtt.local", "port": 1883, "username": "user", "password": "pass"}` | v1, v2 |
| MQTT Reset | - | {topic_prefix}/{storage}/mqtt/reset | - | v2 only |

### Timer Control (V2 Only)

| Description | Read Topic | Write Topic | Value Format Example | Available in version |
|------------|------------|-------------|---------------------|---------------------|
| Set Timer Configuration | - | {topic_prefix}/{storage}/timer/set | `{"enabled": true, "outputPower": 500, "start": {"hour": 8, "minute": 0}, "end": {"hour": 17, "minute": 0}}` | v2 only |

### Device Control

| Description | Read Topic | Write Topic | Value Format Example | Available in version |
|------------|------------|-------------|---------------------|---------------------|
| Device Reboot | - | {topic_prefix}/{storage}/reboot/set | - | v1, v2 |
| Factory Reset | - | {topic_prefix}/{storage}/factory_settings/set | - | v1, v2 |
| Charge Mode | - | {topic_prefix}/{storage}/charge_mode/set | "LoadFirst" or "SimultaneousChargeAndDischarge" (V2) or "PV2Passthrough" (V1) | v1, v2 |
| Discharge Threshold | - | {topic_prefix}/{storage}/discharge_threshold/set | Integer value | v1 only |
| Depth of Discharge | - | {topic_prefix}/{storage}/dod/set | Integer value | v1 only |
| Output Enable | - | {topic_prefix}/{storage}/power{output}/enabled/set | "ON" or "OFF" | v1 only |

### PowerZero Features (Optional)

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| PowerZero Enable | {topic_prefix}/npw/enabled | {topic_prefix}/npw/enabled/set | v1, v2 |
| Maximum Limit | {topic_prefix}/npw/max_limit | {topic_prefix}/npw/max_limit/set | v1, v2 |
| Grid Power Subscription | {grid_power_topic} | - | v1, v2 |
| OpenDTU Limit | {limit_state_topic} | {limit_cmd_topic} | v1, v2 |

### Power Meter Features (Optional)

| Description | Read Topic | Write Topic | Available in version |
|------------|------------|-------------|---------------------|
| Voltage | {topic_prefix}/voltage | - | v1, v2 |
| Current | {topic_prefix}/current | - | v1, v2 |
| Power | {topic_prefix}/power | - | v1, v2 |
| Frequency | {topic_prefix}/frequency | - | v1, v2 |
| Daily Energy | {topic_prefix}/energy_daily | - | v1, v2 |


### Value Formats

#### Cell Voltage Data (b2500/{storage}/cell):
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

#### Runtime Info (b2500/{storage}/runtime):
```json
{
    "in1": {
        "active": true,
        "transparent": false,
        "power": 120
    },
    "in2": {
        "active": true,
        "transparent": false,
        "power": 80
    },
    "soc": 85,
    "capacity": 2000,
    "deviceVersion": "1.2.3",
    "chargeMode": "LoadFirst",
    "out1": {
        "active": true,
        "power": 100,
        "enabled": true
    },
    "out2": {
        "active": true,
        "power": 50,
        "enabled": true
    },
    "dod": 80,
    "wifiConnected": true,
    "mqttConnected": true,
    "dischargeThreshold": 20,
    "scene": "DAY",
    "region": "EU",
    "extern": {
        "connected1": false,
        "connected2": false
    },
    "time": "14:30",
    "temperature": {
        "low": 25,
        "high": 35
    }
}
```

#### Timer Info (b2500/{storage}/timer) - V2 Only:
```json
{
    "adaptiveModeEnabled": true,
    "smartMeter": {
        "connected": true,
        "reading": 1500,
        "power": 800,
        "unknown": 0
    },
    "timer1": {
        "enabled": true,
        "outputPower": 500,
        "start": {
            "hour": 8,
            "minute": 0
        },
        "end": {
            "hour": 17,
            "minute": 0
        }
    }
    // timer2 through timer5 follow same format
}
```

</details>

## Acknowledgments

- Thanks to [@noone2k](https://github.com/noone2k) for reverse engineering the bluetooth protocol and creating the [initial ESPHome config](https://github.com/noone2k/hm2500pub)
- Thanks to the [ESPHome](https://esphome.io/) community for their excellent documentation and tools.
- Thanks to [Material-UI](https://mui.com/) for providing the UI components used in this project.
