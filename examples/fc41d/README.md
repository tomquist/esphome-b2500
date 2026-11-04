# ESPHome on the B2500's own Wi-Fi module

**Experimental, untested on a real battery.**

The B2500 talks to the cloud and the Marstek app through a Quectel FC41D
Wi-Fi/BLE module (a Beken BK7231N) driven by the battery's main MCU over AT
commands. These configs replace the module's firmware with ESPHome, so the
battery needs no external ESP32: it shows up in Home Assistant directly.

| File | What it is |
|---|---|
| `ble-spike.yaml` | BLE test for the FC41D-TE-B eval kit: the battery's `FF00` service, echoing `ff01` writes on `ff02` under Wi-Fi load. |
| `b2500-module.yaml` | The module firmware: answers the battery MCU (`hame_at`), keeps the app's Bluetooth service, forwards the battery's MQTT session to the Hame cloud (switchable, on by default), polls status. |

Components (in `components/`):

- `bk72xx_ble_server`: BLE GATT server (peripheral) for the BK7231N, on
  top of esphome's `bk72xx_ble`. `mac_address:` sets the public BLE address
  (the stock module's own one is in its config sector; ESPHome would
  otherwise use the Wi-Fi MAC + 1).
- `hame_at`: the AT firmware emulator. It covers system, Wi-Fi, BLE, MQTT,
  certificate upload, HTTP and TCP/UDP sockets (the CT and Shelly meter
  path). Optional cloud forwarding uses the broker, credentials and
  certificates the battery MCU sends. Formats and timing follow the hm2500
  firmware emulator, which runs the real MCU images against a module model.
  Host-tested in `tests/hame_at`.

## Cloud access

The battery's traffic to the Hame cloud can be passed through or answered on
the module itself, the way [Marsrelay](https://github.com/tomquist/marsrelay)
does.

- **Master switch.** `cloud:` (also a switch in Home Assistant) turns all
  cloud access on or off. Off means everything is answered on the module, and
  the Marstek app then only works over Bluetooth.
- **Per kind of traffic.** While cloud access is on, `cloud_access:` decides
  for each kind of traffic:

| Key | Covers | Default |
|---|---|---|
| `mqtt` | the battery's broker session: app control, cloud data | `forward` |
| `time` | the battery's clock request | `auto` |
| `device_config` | output limits, SOC, schedules, weather | `auto` |
| `reports` | statistics and error uploads | `forward` |

- **Modes.** `forward` passes the traffic through. `local` answers on the
  module. `auto` passes through and answers locally when the cloud fails
  (HTTP only).
- **Not covered by these settings:**
  - Requests to devices on your network (CT, P1 and Shelly meters) always go
    out.
  - Battery firmware downloads are blocked unless `allow_mcu_ota: true`.

## Bench testing on the eval kit

On the FC41D-TE-B, J201 (M_UART_USB) is the module's main UART (the battery
MCU's side, also used for flashing) and J202 (DEBUG_USB) is the debug UART
(ESPHome logs). On macOS the ports are `/dev/cu.usbserial-…`, on Linux
`/dev/ttyUSB<n>`; `python3 tools/fc41d_probe.py --list` shows them.
`ltchiptool` flashes only the app partition from the UF2, so the module's
bootloader stays; `ltchiptool flash write fc41d-stock.bin` restores the full
backup.

1. Back up the stock firmware. Start the command, then press SW301 when it
   waits for the chip:

   ```bash
   ltchiptool flash read bk7231n fc41d-stock.bin
   ```

2. Create `secrets.yaml` with `wifi_ssid` and `wifi_password`, then build and
   flash:

   ```bash
   esphome compile b2500-module.yaml
   ```

   ```bash
   ltchiptool flash write .esphome/build/b2500-module/.pioenvs/b2500-module/firmware.uf2
   ```

   For `ble-spike.yaml`, check the BLE link from a computer with Bluetooth
   (`pip install bleak`); `--duration 3600` keeps reconnecting for an hour:

   ```bash
   python3 ../../tools/ble_spike_check.py
   ```

3. Play the battery on J201:

   ```bash
   python3 ../../tools/fc41d_mcu_sim.py /dev/cu.usbserial-XXXX
   ```

   Add `--broker host:8883 --ca ca.pem --cert client.pem --key client.key` to
   test cloud forwarding against your own TLS broker.

## On a battery

Not yet. The open steps:

1. Capture the stock module's UART traffic to confirm the response formats.
2. Find a safe way to reach the module.
3. Read the stock module's BLE MAC into `ble_mac`.

See the plan in the hm2500 research repository.

## Known limits

- **Flash.** `b2500-module.yaml` uses about 84% of the app partition.
- **RAM.** RAM use with TLS and BLE running together still has to be
  measured on hardware.
- **Battery firmware updates.** These are blocked unless `allow_mcu_ota: true`.
- **Wi-Fi provisioning.** Setting Wi-Fi from the Marstek app is ignored.
  ESPHome manages Wi-Fi instead.
