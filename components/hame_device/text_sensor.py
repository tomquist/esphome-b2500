import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_KEY, ENTITY_SCHEMA, hame_device_ns, register_listener

DEPENDENCIES = ["hame_device"]

CONF_MAP = "map"
CONF_DEFAULT = "default"
CONF_TIME = "time"

HameDeviceTextSensor = hame_device_ns.class_("HameDeviceTextSensor", text_sensor.TextSensor)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(HameDeviceTextSensor)
    .extend(ENTITY_SCHEMA)
    .extend(
        {
            cv.Required(CONF_KEY): cv.string_strict,
            # raw value -> text; unmapped values show as "unknown (<raw>)".
            cv.Optional(CONF_MAP): cv.Schema({cv.string: cv.string}),
            # Text for values not in map (default: "unknown (<raw>)").
            cv.Optional(CONF_DEFAULT): cv.string,
            # Show "H:M" as "HH:MM".
            cv.Optional(CONF_TIME, default=False): cv.boolean,
        }
    )
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    cg.add(var.set_key(config[CONF_KEY]))
    for raw, label in config.get(CONF_MAP, {}).items():
        cg.add(var.add_mapping(str(raw), label))
    if CONF_DEFAULT in config:
        cg.add(var.set_default(config[CONF_DEFAULT]))
    if config[CONF_TIME]:
        cg.add(var.set_time(True))
    await register_listener(var, config)
