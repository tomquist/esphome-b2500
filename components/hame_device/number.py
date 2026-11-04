import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP

from . import (
    CONF_COMMAND,
    ENTITY_SCHEMA,
    HameDevice,
    NUMBER_FIELD_SCHEMA,
    hame_device_ns,
    number_field_to_code,
    register_listener,
)

DEPENDENCIES = ["hame_device"]

HameDeviceNumber = hame_device_ns.class_("HameDeviceNumber", number.Number, cg.Parented.template(HameDevice))

CONFIG_SCHEMA = (
    number.number_schema(HameDeviceNumber)
    .extend(ENTITY_SCHEMA)
    .extend(NUMBER_FIELD_SCHEMA)
    .extend(
        {
            # {value}: the new value divided by `multiply`.
            cv.Required(CONF_COMMAND): cv.string_strict,
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP, default=1): cv.positive_float,
        }
    )
)


async def to_code(config):
    var = await number.new_number(
        config, min_value=config[CONF_MIN_VALUE], max_value=config[CONF_MAX_VALUE], step=config[CONF_STEP]
    )
    number_field_to_code(var, config)
    cg.add(var.set_command(config[CONF_COMMAND]))
    hub = await register_listener(var, config)
    cg.add(var.set_parent(hub))
