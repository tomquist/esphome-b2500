"""hame_at: ESPHome firmware for the Wi-Fi module inside Hame/Marstek batteries.

Replaces the Quectel FC41D's AT firmware: the battery MCU keeps talking AT over
the UART and this component answers, backed by ESPHome's Wi-Fi, an optional
BLE server (bk72xx_ble_server) and an optional real connection to the Hame
cloud using the broker, credentials and certificates the MCU itself provides.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import time as time_, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_PAYLOAD, CONF_TIME_ID, CONF_TOPIC, CONF_TRIGGER_ID

CODEOWNERS = ["@tomquist"]
DEPENDENCIES = ["uart", "wifi", "bk72xx"]
MULTI_CONF = False

CONF_HAME_AT_ID = "hame_at_id"
CONF_MODULE_VERSION = "module_version"
CONF_BLE_MAC = "ble_mac"
CONF_CLOUD = "cloud"
CONF_ALLOW_MCU_OTA = "allow_mcu_ota"
CONF_BLE_SERVER_ID = "ble_server_id"
CONF_ON_MCU_PUBLISH = "on_mcu_publish"
CONF_ON_BLE_NOTIFY = "on_ble_notify"

hame_at_ns = cg.esphome_ns.namespace("hame_at")
HameAtComponent = hame_at_ns.class_("HameAtComponent", cg.Component, uart.UARTDevice)
SendToMcuAction = hame_at_ns.class_("SendToMcuAction", automation.Action)
BleWriteAction = hame_at_ns.class_("BleWriteAction", automation.Action)

uint8_vector = cg.std_vector.template(cg.uint8)

# Referenced by type only, so hame_at also works without the BLE server.
BK72xxBLEServer = cg.esphome_ns.namespace("bk72xx_ble_server").class_(
    "BK72xxBLEServer", cg.Component
)


def _mac(value):
    value = cv.string_strict(value).upper()
    parts = value.split(":")
    if len(parts) != 6 or any(len(p) != 2 for p in parts):
        raise cv.Invalid("Expected a MAC address like AA:BB:CC:DD:EE:FF")
    for p in parts:
        int(p, 16)
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HameAtComponent),
            # Identity the MCU builds its device id from: copy both from the
            # stock module (dump / UART capture) before flashing.
            cv.Optional(CONF_BLE_MAC): _mac,
            cv.Optional(CONF_MODULE_VERSION, default="FC41DAAR18A04M02_HM"): cv.string_strict,
            # Forward the MCU's MQTT session (and the Hame HTTP endpoints) to
            # the real cloud, so the Marstek app keeps working.
            cv.Optional(CONF_CLOUD, default=True): cv.boolean,
            # Let the MCU download its own firmware updates from the cloud.
            cv.Optional(CONF_ALLOW_MCU_OTA, default=False): cv.boolean,
            cv.Optional(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
            cv.Optional(CONF_BLE_SERVER_ID): cv.use_id(BK72xxBLEServer),
            cv.Optional(CONF_ON_MCU_PUBLISH): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template(cg.std_string, cg.std_string)
                    ),
                }
            ),
            cv.Optional(CONF_ON_BLE_NOTIFY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template(uint8_vector)
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "hame_at", baud_rate=115200, require_tx=True, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_module_version(config[CONF_MODULE_VERSION]))
    if CONF_BLE_MAC in config:
        cg.add(var.set_ble_mac(config[CONF_BLE_MAC]))
    cg.add(var.set_cloud_enabled(config[CONF_CLOUD]))
    cg.add(var.set_allow_mcu_ota(config[CONF_ALLOW_MCU_OTA]))
    if CONF_TIME_ID in config:
        clock = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(clock))
    if CONF_BLE_SERVER_ID in config:
        server = await cg.get_variable(config[CONF_BLE_SERVER_ID])
        cg.add(var.set_ble_server(server))
        # The MCU decides when to advertise and under which name.
        cg.add(server.set_auto_advertise(False))

    for conf in config.get(CONF_ON_MCU_PUBLISH, []):
        trigger = cg.Pvariable(conf[CONF_TRIGGER_ID], var.get_mcu_publish_trigger())
        await automation.build_automation(
            trigger, [(cg.std_string, "topic"), (cg.std_string, "payload")], conf
        )
    for conf in config.get(CONF_ON_BLE_NOTIFY, []):
        trigger = cg.Pvariable(conf[CONF_TRIGGER_ID], var.get_ble_notify_trigger())
        await automation.build_automation(trigger, [(uint8_vector, "x")], conf)

    cg.add_define("USE_HAME_AT")


@automation.register_action(
    "hame_at.send_to_mcu",
    SendToMcuAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HameAtComponent),
            cv.Required(CONF_PAYLOAD): cv.templatable(cv.string),
            cv.Optional(CONF_TOPIC): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def send_to_mcu_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    payload = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(payload))
    if CONF_TOPIC in config:
        topic = await cg.templatable(config[CONF_TOPIC], args, cg.std_string)
        cg.add(var.set_topic(topic))
    return var


@automation.register_action(
    "hame_at.ble_write",
    BleWriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HameAtComponent),
            cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        }
    ),
    synchronous=True,
)
async def ble_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]
    if cg.is_template(data):
        cg.add(var.set_data(await cg.templatable(data, args, uint8_vector)))
    else:
        cg.add(var.set_data(data))
    return var
