import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv

from . import (
    CONF_COMMAND,
    CONF_HAME_DEVICE_ID,
    CONF_KEY,
    ENTITY_SCHEMA,
    HameDevice,
    hame_device_ns,
    keep_template_keys,
    register_listener,
)

DEPENDENCIES = ["hame_device"]

CONF_TIME = "time"

HameDeviceText = hame_device_ns.class_("HameDeviceText", text.Text, cg.Parented.template(HameDevice))

CONFIG_SCHEMA = (
    text.text_schema(HameDeviceText, mode="TEXT")
    .extend(ENTITY_SCHEMA)
    .extend(
        {
            cv.Required(CONF_KEY): cv.string_strict,
            # {value}: the entered text ("H:M" with `time`).
            cv.Required(CONF_COMMAND): cv.string_strict,
            # A time of day: shown "HH:MM", sent "H:M", anything else refused.
            cv.Optional(CONF_TIME, default=False): cv.boolean,
        }
    )
)


async def to_code(config):
    pattern = "^([01]?[0-9]|2[0-3]):[0-5][0-9]$" if config[CONF_TIME] else None
    var = await text.new_text(config, min_length=0, max_length=32, pattern=pattern)
    cg.add(var.set_key(config[CONF_KEY]))
    cg.add(var.set_command(config[CONF_COMMAND]))
    await keep_template_keys(config[CONF_HAME_DEVICE_ID], config[CONF_COMMAND])
    if config[CONF_TIME]:
        cg.add(var.set_time(True))
    hub = await register_listener(var, config)
    cg.add(var.set_parent(hub))
