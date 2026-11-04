"""BLE GATT server (peripheral) for the BK7231N, on top of esphome's bk72xx_ble.

esphome ships the BK72xx BLE controller and a scanner, but no peripheral role.
The Beken BDK has a complete GATT-server API (bk_ble_create_db, advertising,
notifications, write/read notices), which this component wraps for one primary
service with 16-bit UUIDs, enough to recreate the FF00 service the Hame
battery firmware publishes through its Quectel FC41D module.

The BDK has a single notice callback. bk72xx_ble registers it for scan
reports; this component takes it over once the stack is up, so it cannot be
combined with bk72xx_ble_tracker.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import bk72xx_ble
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATA,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NAME,
    CONF_TRIGGER_ID,
    CONF_UUID,
)
import esphome.final_validate as fv

CODEOWNERS = ["@tomquist"]
DEPENDENCIES = ["bk72xx"]
AUTO_LOAD = ["bk72xx_ble"]

CONF_BK72XX_BLE_ID = "bk72xx_ble_id"
CONF_SERVICE_UUID = "service_uuid"
CONF_CHARACTERISTICS = "characteristics"
CONF_READ = "read"
CONF_WRITE = "write"
CONF_WRITE_NO_RESPONSE = "write_no_response"
CONF_NOTIFY = "notify"
CONF_ON_WRITE = "on_write"
CONF_ON_CONNECT = "on_connect"
CONF_ON_DISCONNECT = "on_disconnect"
CONF_ADVERTISING_INTERVAL = "advertising_interval"
CONF_AUTO_ADVERTISE = "auto_advertise"

bk72xx_ble_server_ns = cg.esphome_ns.namespace("bk72xx_ble_server")
BK72xxBLEServer = bk72xx_ble_server_ns.class_("BK72xxBLEServer", cg.Component)
BLECharacteristic = bk72xx_ble_server_ns.class_("BLECharacteristic")
NotifyAction = bk72xx_ble_server_ns.class_("NotifyAction", automation.Action)
SetValueAction = bk72xx_ble_server_ns.class_("SetValueAction", automation.Action)
ConnectedCondition = bk72xx_ble_server_ns.class_(
    "ConnectedCondition", automation.Condition
)

uint8_vector = cg.std_vector.template(cg.uint8)

uuid16 = cv.All(cv.hex_int, cv.int_range(min=1, max=0xFFFF))


def _validate_characteristic(config):
    if not (
        config[CONF_READ]
        or config[CONF_WRITE]
        or config[CONF_WRITE_NO_RESPONSE]
        or config[CONF_NOTIFY]
    ):
        raise cv.Invalid("A characteristic needs at least one property")
    if CONF_ON_WRITE in config and not (
        config[CONF_WRITE] or config[CONF_WRITE_NO_RESPONSE]
    ):
        raise cv.Invalid("on_write needs write or write_no_response")
    return config


CHARACTERISTIC_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLECharacteristic),
            cv.Required(CONF_UUID): uuid16,
            cv.Optional(CONF_READ, default=False): cv.boolean,
            cv.Optional(CONF_WRITE, default=False): cv.boolean,
            cv.Optional(CONF_WRITE_NO_RESPONSE, default=False): cv.boolean,
            cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
            cv.Optional(CONF_ON_WRITE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template(uint8_vector)
                    ),
                }
            ),
        }
    ),
    _validate_characteristic,
)


def _unique_uuids(chars):
    seen = set()
    for c in chars:
        if c[CONF_UUID] in seen:
            raise cv.Invalid(f"Duplicate characteristic UUID 0x{c[CONF_UUID]:04X}")
        seen.add(c[CONF_UUID])
    return chars


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BK72xxBLEServer),
        cv.GenerateID(CONF_BK72XX_BLE_ID): cv.use_id(bk72xx_ble.BK72xxBLE),
        cv.Required(CONF_NAME): cv.All(cv.string_strict, cv.Length(max=26)),
        cv.Optional(CONF_SERVICE_UUID, default=0xFF00): uuid16,
        cv.Optional(
            CONF_ADVERTISING_INTERVAL, default="100ms"
        ): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(
                min=cv.TimePeriod(milliseconds=20), max=cv.TimePeriod(milliseconds=10240)
            ),
        ),
        # Advertise as soon as the service exists. Off: only after
        # start_advertising() (hame_at lets the battery MCU decide).
        cv.Optional(CONF_AUTO_ADVERTISE, default=True): cv.boolean,
        # Public BLE address. Default: the BDK's, the Wi-Fi MAC with the last
        # byte + 1. A Quectel FC41D keeps its own in the config sector, which
        # the battery app may know the module by.
        cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Required(CONF_CHARACTERISTICS): cv.All(
            cv.ensure_list(CHARACTERISTIC_SCHEMA), cv.Length(min=1, max=8), _unique_uuids
        ),
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation({}),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation({}),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config):
    if "bk72xx_ble_tracker" in fv.full_config.get():
        raise cv.Invalid(
            "bk72xx_ble_server takes over the BDK notice callback and cannot be "
            "combined with bk72xx_ble_tracker"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    ble = await cg.get_variable(config[CONF_BK72XX_BLE_ID])
    cg.add(var.set_ble(ble))
    cg.add(var.set_name(config[CONF_NAME]))
    cg.add(var.set_service_uuid(config[CONF_SERVICE_UUID]))
    cg.add(var.set_auto_advertise(config[CONF_AUTO_ADVERTISE]))
    # BLE advertising interval unit: 0.625 ms.
    cg.add(
        var.set_advertising_interval(
            int(config[CONF_ADVERTISING_INTERVAL].total_milliseconds / 0.625)
        )
    )

    # Declare every characteristic before building any automation, so an
    # on_write of one characteristic can notify another.
    chr_vars = []
    for chr_conf in config[CONF_CHARACTERISTICS]:
        chr_var = cg.new_Pvariable(chr_conf[CONF_ID], chr_conf[CONF_UUID])
        cg.add(
            chr_var.set_properties(
                chr_conf[CONF_READ],
                chr_conf[CONF_WRITE],
                chr_conf[CONF_WRITE_NO_RESPONSE],
                chr_conf[CONF_NOTIFY],
            )
        )
        cg.add(var.add_characteristic(chr_var))
        chr_vars.append((chr_var, chr_conf))

    for chr_var, chr_conf in chr_vars:
        for conf in chr_conf.get(CONF_ON_WRITE, []):
            trigger = cg.Pvariable(
                conf[CONF_TRIGGER_ID], chr_var.get_on_write_trigger()
            )
            await automation.build_automation(trigger, [(uint8_vector, "x")], conf)

    for conf in config.get(CONF_ON_CONNECT, []):
        await automation.build_automation(var.get_on_connect_trigger(), [], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        await automation.build_automation(var.get_on_disconnect_trigger(), [], conf)

    cg.add_define("USE_BK72XX_BLE_SERVER")
    if CONF_MAC_ADDRESS in config:
        # The BDK takes the BLE address from wifi_get_mac_address() in
        # bdaddr_env_init(); bdaddr_override.c answers that one call.
        mac = config[CONF_MAC_ADDRESS].parts
        cg.add_define(
            "USE_BK72XX_BLE_SERVER_MAC",
            cg.RawExpression("{" + ",".join(f"0x{b:02X}" for b in mac) + "}"),
        )
        cg.add_build_flag("-Wl,--wrap=wifi_get_mac_address")


DATA_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
        cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
    }
)


async def _data_action(config, action_id, template_arg, args):
    chr_var = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, chr_var)
    data = config[CONF_DATA]
    if cg.is_template(data):
        templ = await cg.templatable(data, args, uint8_vector)
        cg.add(var.set_data(templ))
    else:
        cg.add(var.set_data(data))
    return var


@automation.register_action(
    "bk72xx_ble_server.notify", NotifyAction, DATA_ACTION_SCHEMA, synchronous=True
)
async def notify_action_to_code(config, action_id, template_arg, args):
    return await _data_action(config, action_id, template_arg, args)


@automation.register_action(
    "bk72xx_ble_server.set_value", SetValueAction, DATA_ACTION_SCHEMA, synchronous=True
)
async def set_value_action_to_code(config, action_id, template_arg, args):
    return await _data_action(config, action_id, template_arg, args)


@automation.register_condition(
    "bk72xx_ble_server.connected",
    ConnectedCondition,
    cv.Schema({cv.GenerateID(): cv.use_id(BK72xxBLEServer)}),
)
async def connected_condition_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
