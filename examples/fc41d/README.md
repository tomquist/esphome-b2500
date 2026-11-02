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
  top of esphome's `bk72xx_ble`.
- `hame_at`: the AT firmware emulator. It covers system, Wi-Fi, BLE, MQTT,
  certificate upload and HTTP. Optional cloud forwarding uses the broker,
  credentials and certificates the battery MCU sends. Host-tested in
  `tests/hame_at`.

## Bench testing on the eval kit

On the FC41D-TE-B, J201 is the module's main UART (the battery MCU's side)
and J202 is the debug UART (ESPHome logs).

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

3. Play the battery on J201:

   ```bash
   python3 ../../tools/fc41d_mcu_sim.py /dev/ttyUSB0
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

- **Flash.** `b2500-module.yaml` uses about 82% of the app partition.
- **RAM.** RAM use with TLS and BLE running together still has to be
  measured on hardware.
- **Battery firmware updates.** These are blocked unless `allow_mcu_ota: true`.
- **Wi-Fi provisioning.** Setting Wi-Fi from the Marstek app is ignored.
  ESPHome manages Wi-Fi instead.
