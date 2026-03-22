
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

## ESPHome Component Documentation

The `b2500` component is a custom [ESPHome](https://esphome.io/) external component for monitoring and controlling B2500 portable power storage devices over Bluetooth Low Energy (BLE). It supports two device generations (V1 and V2) and exposes sensors, binary sensors, switches, selects, numbers, buttons, text sensors, and datetime entities to [Home Assistant](https://www.home-assistant.io/).

The component follows a hub pattern: a central `b2500` hub communicates with the device via BLE, and platform sub-components (sensor, switch, etc.) attach to it. Each platform block requires a `generation` key set to `1` or `2`, which controls which entities are available.

```yaml
# Example: loading the external component
external_components:
  - source: github://tomquist/esphome-b2500
    components: [b2500]
```

### Base Component

The `b2500` hub component handles BLE communication and polling. It requires the [`esp32_ble_tracker`](https://esphome.io/components/esp32_ble_tracker) and [`ble_client`](https://esphome.io/components/ble_client) components, as well as a [`time`](https://esphome.io/components/time/) component for clock synchronization.

```yaml
# Example configuration entry
esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: my_ble_client

time:
  - platform: sntp
    id: sntp_time

b2500:
  id: b2500_device
  generation: 2
```

#### Configuration variables

- **generation** (**Required**, int): The device generation. Must be `1` or `2`.
- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): Manually specify the component ID.
- **update_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): The interval to poll the device. Defaults to `10s`.
- **time_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of a `time` component for clock synchronization. Automatically detected if only one time component exists.
- All other options from [BLE Client](https://esphome.io/components/ble_client).

#### Automation Triggers

- **on_device_info** (*Optional*, [Automation](https://esphome.io/automations/)): Triggered when device information is received from the B2500. The trigger variable `x` contains device info fields.
- **on_runtime_info** (*Optional*, [Automation](https://esphome.io/automations/)): Triggered when runtime information (power, SOC, temperatures, etc.) is received. The trigger variable `x` contains runtime fields.
- **on_cell_info** (*Optional*, [Automation](https://esphome.io/automations/)): Triggered when battery cell information is received. The trigger variable `x` contains cell voltage data.
- **on_wifi_info** (*Optional*, [Automation](https://esphome.io/automations/)): Triggered when WiFi information is received. The trigger variable `x` contains WiFi status fields.
- **on_fc41d_info** (*Optional*, [Automation](https://esphome.io/automations/)): Triggered when FC41D module information is received. The trigger variable `x` contains FC41D firmware data.
- **on_timer_info** (*Optional*, [Automation](https://esphome.io/automations/)): **V2 only.** Triggered when timer configuration is received. The trigger variable `x` contains timer settings.

```yaml
# Example: using automation triggers
b2500:
  id: b2500_device
  generation: 2
  on_runtime_info:
    then:
      - logger.log: "Runtime info received"
  on_timer_info:
    then:
      - logger.log: "Timer info received"
```

### Sensor

The `b2500` sensor platform exposes power, battery, and temperature measurements.

```yaml
# Example configuration entry
sensor:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    soc:
      name: "Battery SOC"
    in1_power:
      name: "PV Input 1"
    in2_power:
      name: "PV Input 2"
    out_total_power:
      name: "Total Output"
    capacity:
      name: "Remaining Capacity"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component. Can be omitted if only one `b2500` hub exists.
- **publish_unchanged_sensor_values** (*Optional*, boolean): Whether to publish sensor updates even when the value has not changed. Defaults to `true`.

#### Sensors (all generations)

- **soc** (*Optional*): Battery state of charge. Unit: `%`, device class: `battery`, state class: `measurement`, accuracy: 1 decimal.
- **in1_power** (*Optional*): PV input 1 power. Unit: `W`, device class: `power`, state class: `measurement`.
- **in2_power** (*Optional*): PV input 2 power. Unit: `W`, device class: `power`, state class: `measurement`.
- **in_total_power** (*Optional*): Total PV input power. Unit: `W`, device class: `power`, state class: `measurement`.
- **out1_power** (*Optional*): Output 1 power. Unit: `W`, device class: `power`, state class: `measurement`.
- **out2_power** (*Optional*): Output 2 power. Unit: `W`, device class: `power`, state class: `measurement`.
- **out_total_power** (*Optional*): Total output power. Unit: `W`, device class: `power`, state class: `measurement`.
- **capacity** (*Optional*): Remaining battery capacity. Unit: `Wh`, device class: `energy_storage`, state class: `measurement`.
- **wifi_rssi** (*Optional*): WiFi signal strength. Device class: `signal_strength`, state class: `measurement`.
- **temperature_low** (*Optional*): Lower temperature reading. Unit: `°C`, device class: `temperature`, state class: `measurement`.
- **temperature_high** (*Optional*): Upper temperature reading. Unit: `°C`, device class: `temperature`, state class: `measurement`.

All sensor options from [Sensor](https://esphome.io/components/sensor/) are supported for each entry.

#### Sensors (V2 only)

- **adaptive_power_out** (*Optional*): Adaptive mode output power. Unit: `W`, device class: `power`, state class: `measurement`.
- **smart_meter_reading** (*Optional*): Smart meter reading. Unit: `VA`, state class: `measurement`.
- **daily_total_battery_charge** (*Optional*): Daily total battery charge. Unit: `Wh`, device class: `energy`, state class: `total_increasing`.
- **daily_total_battery_discharge** (*Optional*): Daily total battery discharge. Unit: `Wh`, device class: `energy`, state class: `total_increasing`.
- **daily_total_load_charge** (*Optional*): Daily total load charge. Unit: `Wh`, device class: `energy`, state class: `total_increasing`.
- **daily_total_load_discharge** (*Optional*): Daily total load discharge. Unit: `Wh`, device class: `energy`, state class: `total_increasing`.

### Binary Sensor

The `b2500` binary sensor platform exposes connectivity and input/output status.

```yaml
# Example configuration entry
binary_sensor:
  - platform: b2500
    b2500_id: b2500_device
    generation: 1
    wifi_connected:
      name: "WiFi Connected"
    in1_active:
      name: "PV1 Active"
    out1_active:
      name: "Output 1 Active"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.

#### Binary sensors (all generations)

- **wifi_connected** (*Optional*): Whether the device's WiFi is connected.
- **mqtt_connected** (*Optional*): Whether the device's MQTT client is connected.
- **in1_active** (*Optional*): Whether PV input 1 is active (producing power).
- **in2_active** (*Optional*): Whether PV input 2 is active (producing power).
- **in1_transparent** (*Optional*): Whether PV input 1 is in transparent pass-through mode.
- **in2_transparent** (*Optional*): Whether PV input 2 is in transparent pass-through mode.
- **out1_active** (*Optional*): Whether output 1 is active (delivering power).
- **out2_active** (*Optional*): Whether output 2 is active (delivering power).
- **extern1_connected** (*Optional*): Whether external battery module 1 is connected.
- **extern2_connected** (*Optional*): Whether external battery module 2 is connected.

All options from [Binary Sensor](https://esphome.io/components/binary_sensor/) are supported for each entry.

#### Binary sensors (V2 only)

- **smart_meter_connected** (*Optional*): Whether a smart meter is connected.

### Switch

The `b2500` switch platform provides controllable switches. The available switches differ between V1 and V2 devices.

```yaml
# Example configuration entry (V1)
switch:
  - platform: b2500
    b2500_id: b2500_device
    generation: 1
    out1:
      name: "Output 1 Enabled"
    out2:
      name: "Output 2 Enabled"
```

```yaml
# Example configuration entry (V2)
switch:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    adaptive_mode:
      name: "Adaptive Mode"
    surplus_feed_in:
      name: "Surplus Feed-in"
    timer1_enabled:
      name: "Timer 1"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.

#### Switches (V1 only)

- **out1** (*Optional*): Enable or disable output 1. Entity category: `config`.
- **out2** (*Optional*): Enable or disable output 2. Entity category: `config`.

#### Switches (V2 only)

- **adaptive_mode** (*Optional*): Enable or disable adaptive power mode. Entity category: `config`.
- **surplus_feed_in** (*Optional*): Enable or disable surplus feed-in to grid. Entity category: `config`.
- **timer1_enabled** (*Optional*): Enable or disable timer 1. Entity category: `config`.
- **timer2_enabled** (*Optional*): Enable or disable timer 2. Entity category: `config`.
- **timer3_enabled** (*Optional*): Enable or disable timer 3. Entity category: `config`.
- **timer4_enabled** (*Optional*): Enable or disable timer 4. Entity category: `config`.
- **timer5_enabled** (*Optional*): Enable or disable timer 5. Entity category: `config`.

All options from [Switch](https://esphome.io/components/switch/) are supported for each entry.

### Select

The `b2500` select platform provides a dropdown for the battery charge mode.

```yaml
# Example configuration entry
select:
  - platform: b2500
    b2500_id: b2500_device
    generation: 1
    charge_mode:
      name: "Charge Mode"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.
- **charge_mode** (*Optional*): Battery charge mode selection. Available options depend on generation:
  - **V1**: `load_first`, `pv2_passthrough`
  - **V2**: `load_first`, `simultaneous_charge_and_discharge`

All options from [Select](https://esphome.io/components/select/) are supported.

### Number

The `b2500` number platform provides adjustable numeric settings.

```yaml
# Example configuration entry (V1)
number:
  - platform: b2500
    b2500_id: b2500_device
    generation: 1
    dod:
      name: "Depth of Discharge"
    discharge_threshold:
      name: "Discharge Threshold"
```

```yaml
# Example configuration entry (V2)
number:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    dod:
      name: "Depth of Discharge"
    timer1_output_power:
      name: "Timer 1 Output Power"
    timer2_output_power:
      name: "Timer 2 Output Power"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.

#### Numbers (all generations)

- **dod** (*Optional*): Depth of discharge — the maximum percentage of the battery that can be used. Range: `0`–`100`, step: `1`, unit: `%`. Entity category: `config`.

#### Numbers (V1 only)

- **discharge_threshold** (*Optional*): Minimum power threshold for discharge. Range: `0`–`999`, step: `1`, unit: `W`. Entity category: `config`.

#### Numbers (V2 only)

- **timer1_output_power** (*Optional*): Output power for timer 1. Range: `50`–`800`, step: `1`, unit: `W`. Entity category: `config`.
- **timer2_output_power** (*Optional*): Output power for timer 2. Range: `50`–`800`, step: `1`, unit: `W`. Entity category: `config`.
- **timer3_output_power** (*Optional*): Output power for timer 3. Range: `50`–`800`, step: `1`, unit: `W`. Entity category: `config`.
- **timer4_output_power** (*Optional*): Output power for timer 4. Range: `50`–`800`, step: `1`, unit: `W`. Entity category: `config`.
- **timer5_output_power** (*Optional*): Output power for timer 5. Range: `50`–`800`, step: `1`, unit: `W`. Entity category: `config`.

All options from [Number](https://esphome.io/components/number/) are supported for each entry.

### Button

The `b2500` button platform provides device control buttons.

```yaml
# Example configuration entry
button:
  - platform: b2500
    b2500_id: b2500_device
    generation: 1
    reboot:
      name: "Reboot"
    factory_reset:
      name: "Factory Reset"
    hardware_reset:
      name: "Hardware Reset"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.

#### Buttons (all generations)

- **reboot** (*Optional*): Reboot the B2500 device.
- **factory_reset** (*Optional*): Reset the B2500 device to factory defaults.
- **hardware_reset** (*Optional*): Reset the BLE connection hardware on the B2500.

All options from [Button](https://esphome.io/components/button/) are supported for each entry.

### Text Sensor

The `b2500` text sensor platform exposes device information as text values.

```yaml
# Example configuration entry
text_sensor:
  - platform: b2500
    b2500_id: b2500_device
    generation: 1
    firmware_version:
      name: "Firmware Version"
    device_id:
      name: "Device ID"
    cell_voltage:
      name: "Cell Voltage"
```

#### Configuration variables

- **generation** (**Required**, int): `1` or `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.

#### Text sensors (all generations)

- **firmware_version** (*Optional*): The device firmware version string. Entity category: `diagnostic`.
- **device_type** (*Optional*): The device type identifier. Entity category: `diagnostic`.
- **device_id** (*Optional*): The unique device ID. Entity category: `diagnostic`.
- **mac_address** (*Optional*): The device BLE MAC address. Entity category: `diagnostic`.
- **fc41d_version** (*Optional*): The FC41D module firmware version. Entity category: `diagnostic`.
- **wifi_ssid** (*Optional*): The WiFi network the device is connected to. Entity category: `diagnostic`.
- **scene** (*Optional*): The current scene, e.g. `DAY` or `NIGHT`. Entity category: `diagnostic`.
- **region** (*Optional*): The configured device region. Entity category: `diagnostic`.
- **cell_voltage** (*Optional*): Battery cell voltage data as JSON. Contains `cells` array, `min`, `max`, `avg`, `sum`, and `delta` values. Entity category: `diagnostic`.
- **last_response** (*Optional*): Timestamp of the last received BLE response. Entity category: `diagnostic`.

#### Text sensors (V2 only)

- **device_time** (*Optional*): The current time reported by the device. Entity category: `diagnostic`.

All options from [Text Sensor](https://esphome.io/components/text_sensor/) are supported for each entry.

### DateTime (V2 Only)

The `b2500` datetime platform exposes timer start and end times as time entities. This platform is only available for V2 devices.

```yaml
# Example configuration entry
datetime:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    timer1_start:
      name: "Timer 1 Start"
    timer1_end:
      name: "Timer 1 End"
    timer2_start:
      name: "Timer 2 Start"
    timer2_end:
      name: "Timer 2 End"
```

#### Configuration variables

- **generation** (**Required**, int): Must be `2`.
- **b2500_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the parent `b2500` component.
- **timer1_start** / **timer1_end** (*Optional*): Start and end time for timer 1.
- **timer2_start** / **timer2_end** (*Optional*): Start and end time for timer 2.
- **timer3_start** / **timer3_end** (*Optional*): Start and end time for timer 3.
- **timer4_start** / **timer4_end** (*Optional*): Start and end time for timer 4.
- **timer5_start** / **timer5_end** (*Optional*): Start and end time for timer 5.

All options from [Datetime](https://esphome.io/components/datetime/) are supported for each entry.

### Actions

The `b2500` component registers several [actions](https://esphome.io/automations/actions) for use in automations. All templatable parameters support [lambdas](https://esphome.io/automations/templates).

#### `b2500.set_wifi` Action

Configure the WiFi credentials on the B2500 device.

```yaml
on_...:
  then:
    - b2500.set_wifi:
        id: b2500_device
        ssid: "MyNetwork"
        password: "MyPassword"
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to control.
- **ssid** (**Required**, string, [templatable](https://esphome.io/automations/templates)): The WiFi SSID.
- **password** (**Required**, string, [templatable](https://esphome.io/automations/templates)): The WiFi password.

#### `b2500.set_mqtt` Action

Configure the MQTT connection on the B2500 device.

```yaml
on_...:
  then:
    - b2500.set_mqtt:
        id: b2500_device
        host: "mqtt.local"
        port: 1883
        username: "user"
        password: "pass"
        ssl: false
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to control.
- **host** (**Required**, string, [templatable](https://esphome.io/automations/templates)): The MQTT broker hostname.
- **port** (**Required**, int, [templatable](https://esphome.io/automations/templates)): The MQTT broker port.
- **username** (**Required**, string, [templatable](https://esphome.io/automations/templates)): The MQTT username.
- **password** (**Required**, string, [templatable](https://esphome.io/automations/templates)): The MQTT password.
- **ssl** (*Optional*, boolean): Whether to use SSL. Defaults to `false`.

#### `b2500.reset_mqtt` Action

Reset the MQTT configuration on the B2500 device to defaults.

```yaml
on_...:
  then:
    - b2500.reset_mqtt:
        id: b2500_device
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to control.

#### `b2500.set_datetime` Action

Synchronize the B2500 device clock.

```yaml
on_...:
  then:
    - b2500.set_datetime:
        id: b2500_device
        datetime: !lambda 'return id(sntp_time).now();'
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to control.
- **datetime** (**Required**, datetime): The date and time to set. Accepts a lambda returning an `ESPTime` struct or a static datetime value.

#### `b2500.reboot` Action

Reboot the B2500 device.

```yaml
on_...:
  then:
    - b2500.reboot:
        id: b2500_device
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to reboot.

#### `b2500.factory_reset` Action

Reset the B2500 device to factory defaults.

```yaml
on_...:
  then:
    - b2500.factory_reset:
        id: b2500_device
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to reset.

#### `b2500.set_dod` Action

Set the depth of discharge (maximum usable battery percentage).

```yaml
on_...:
  then:
    - b2500.set_dod:
        id: b2500_device
        dod: 80
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to control.
- **dod** (**Required**, int, [templatable](https://esphome.io/automations/templates)): The depth of discharge percentage (0–100).

#### `b2500.set_charge_mode` Action

Set the battery charge mode.

```yaml
on_...:
  then:
    - b2500.set_charge_mode:
        id: b2500_device
        charge_mode: load_first
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` component to control.
- **charge_mode** (**Required**, enum, [templatable](https://esphome.io/automations/templates)): One of `load_first`, `pv2_passthrough` (V1), or `simultaneous_charge_and_discharge` (V2).

#### `b2500.set_out_active` Action (V1 Only)

Enable or disable a power output on V1 devices.

```yaml
on_...:
  then:
    - b2500.set_out_active:
        id: b2500_device
        generation: 1
        output: 1
        active: true
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` V1 component to control.
- **generation** (**Required**, int): Must be `1`.
- **output** (**Required**, int, [templatable](https://esphome.io/automations/templates)): The output number (`1` or `2`).
- **active** (**Required**, boolean, [templatable](https://esphome.io/automations/templates)): Whether to enable (`true`) or disable (`false`) the output.

#### `b2500.set_discharge_threshold` Action (V1 Only)

Set the minimum power threshold for discharge on V1 devices.

```yaml
on_...:
  then:
    - b2500.set_discharge_threshold:
        id: b2500_device
        generation: 1
        threshold: 100
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` V1 component to control.
- **generation** (**Required**, int): Must be `1`.
- **threshold** (**Required**, int, [templatable](https://esphome.io/automations/templates)): The discharge threshold in watts (0–999).

#### `b2500.set_timer` Action (V2 Only)

Configure a timer on V2 devices. At least one optional field besides `timer` must be provided. Fields that are not specified retain their current value.

```yaml
on_...:
  then:
    - b2500.set_timer:
        id: b2500_device
        generation: 2
        timer: 1
        enabled: true
        output_power: 500
        start_hour: 8
        start_minute: 0
        end_hour: 17
        end_minute: 0
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` V2 component to control.
- **generation** (**Required**, int): Must be `2`.
- **timer** (**Required**, int, [templatable](https://esphome.io/automations/templates)): Timer index (`0`–`4`, corresponding to timers 1–5).
- **enabled** (*Optional*, boolean, [templatable](https://esphome.io/automations/templates)): Whether the timer is enabled.
- **output_power** (*Optional*, int, [templatable](https://esphome.io/automations/templates)): Timer output power in watts.
- **start_hour** (*Optional*, int, [templatable](https://esphome.io/automations/templates)): Timer start hour.
- **start_minute** (*Optional*, int, [templatable](https://esphome.io/automations/templates)): Timer start minute.
- **end_hour** (*Optional*, int, [templatable](https://esphome.io/automations/templates)): Timer end hour.
- **end_minute** (*Optional*, int, [templatable](https://esphome.io/automations/templates)): Timer end minute.

#### `b2500.set_adaptive_mode_enabled` Action (V2 Only)

Enable or disable adaptive power mode on V2 devices.

```yaml
on_...:
  then:
    - b2500.set_adaptive_mode_enabled:
        id: b2500_device
        generation: 2
        enabled: true
```

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types#config-id)): The `b2500` V2 component to control.
- **generation** (**Required**, int): Must be `2`.
- **enabled** (**Required**, boolean, [templatable](https://esphome.io/automations/templates)): Whether to enable (`true`) or disable (`false`) adaptive mode.

### Complete Example

A full configuration for an ESP32-S3 with a V2 B2500 device, exposing all available entities:

```yaml
external_components:
  - source: github://tomquist/esphome-b2500
    components: [b2500]

esphome:
  name: b2500-gateway

esp32:
  board: esp32-s3-devkitc-1

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:

api:

esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: my_ble_client

time:
  - platform: sntp
    id: sntp_time

b2500:
  id: b2500_device
  generation: 2
  on_runtime_info:
    then:
      - logger.log: "Runtime info updated"

sensor:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    soc:
      name: "Battery SOC"
    in1_power:
      name: "PV Input 1 Power"
    in2_power:
      name: "PV Input 2 Power"
    in_total_power:
      name: "Total Input Power"
    out1_power:
      name: "Output 1 Power"
    out2_power:
      name: "Output 2 Power"
    out_total_power:
      name: "Total Output Power"
    capacity:
      name: "Remaining Capacity"
    temperature_low:
      name: "Temperature Low"
    temperature_high:
      name: "Temperature High"
    adaptive_power_out:
      name: "Adaptive Output Power"
    smart_meter_reading:
      name: "Smart Meter Reading"

binary_sensor:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    wifi_connected:
      name: "WiFi Connected"
    mqtt_connected:
      name: "MQTT Connected"
    in1_active:
      name: "PV1 Active"
    in2_active:
      name: "PV2 Active"
    out1_active:
      name: "Output 1 Active"
    out2_active:
      name: "Output 2 Active"
    smart_meter_connected:
      name: "Smart Meter Connected"

switch:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    adaptive_mode:
      name: "Adaptive Mode"
    surplus_feed_in:
      name: "Surplus Feed-in"
    timer1_enabled:
      name: "Timer 1 Enabled"
    timer2_enabled:
      name: "Timer 2 Enabled"

select:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    charge_mode:
      name: "Charge Mode"

number:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    dod:
      name: "Depth of Discharge"
    timer1_output_power:
      name: "Timer 1 Output Power"
    timer2_output_power:
      name: "Timer 2 Output Power"

button:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    reboot:
      name: "Reboot"
    factory_reset:
      name: "Factory Reset"

text_sensor:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    firmware_version:
      name: "Firmware Version"
    device_id:
      name: "Device ID"
    cell_voltage:
      name: "Cell Voltage"

datetime:
  - platform: b2500
    b2500_id: b2500_device
    generation: 2
    timer1_start:
      name: "Timer 1 Start"
    timer1_end:
      name: "Timer 1 End"
    timer2_start:
      name: "Timer 2 Start"
    timer2_end:
      name: "Timer 2 End"
```

### Running tests

To verify the ESPHome component compiles correctly, install the
Python dependencies and run `pytest`:

```bash
pip install esphome pytest
pytest
```

## Acknowledgments

- Thanks to [@noone2k](https://github.com/noone2k) for reverse engineering the bluetooth protocol and creating the [initial ESPHome config](https://github.com/noone2k/hm2500pub)
- Thanks to the [ESPHome](https://esphome.io/) community for their excellent documentation and tools.
- Thanks to [Material-UI](https://mui.com/) for providing the UI components used in this project.
