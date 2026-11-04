import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from . import BOOL_FIELD_SCHEMA, ENTITY_SCHEMA, bool_field_to_code, hame_device_ns, register_listener

DEPENDENCIES = ["hame_device"]

HameDeviceBinarySensor = hame_device_ns.class_("HameDeviceBinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(HameDeviceBinarySensor).extend(ENTITY_SCHEMA).extend(BOOL_FIELD_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    bool_field_to_code(var, config)
    await register_listener(var, config)
