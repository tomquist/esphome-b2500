import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from . import ENTITY_SCHEMA, NUMBER_FIELD_SCHEMA, hame_device_ns, number_field_to_code, register_listener

DEPENDENCIES = ["hame_device"]

HameDeviceSensor = hame_device_ns.class_("HameDeviceSensor", sensor.Sensor)

CONF_MONOTONIC = "monotonic"

CONFIG_SCHEMA = (
    sensor.sensor_schema(HameDeviceSensor)
    .extend(ENTITY_SCHEMA)
    .extend(NUMBER_FIELD_SCHEMA)
    .extend(
        {
            # Daily counters: ignore one lower reading (a glitch); the second
            # in a row is the daily reset.
            cv.Optional(CONF_MONOTONIC, default=False): cv.boolean,
        }
    )
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    number_field_to_code(var, config)
    if config[CONF_MONOTONIC]:
        cg.add(var.set_monotonic(True))
    await register_listener(var, config)
