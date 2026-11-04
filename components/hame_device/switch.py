import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from . import (
    BOOL_FIELD_SCHEMA,
    CONF_COMMAND,
    ENTITY_SCHEMA,
    HameDevice,
    bool_field_to_code,
    hame_device_ns,
    register_listener,
)

DEPENDENCIES = ["hame_device"]

CONF_COMMAND_ON = "command_on"
CONF_COMMAND_OFF = "command_off"

HameDeviceSwitch = hame_device_ns.class_("HameDeviceSwitch", switch.Switch, cg.Parented.template(HameDevice))


def _commands(config):
    if CONF_COMMAND in config:
        if CONF_COMMAND_ON in config or CONF_COMMAND_OFF in config:
            raise cv.Invalid("Use either command ({value} = 1/0) or command_on and command_off")
        return config
    if CONF_COMMAND_ON not in config or CONF_COMMAND_OFF not in config:
        raise cv.Invalid("Set command, or both command_on and command_off")
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(HameDeviceSwitch)
    .extend(ENTITY_SCHEMA)
    .extend(BOOL_FIELD_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_COMMAND): cv.string_strict,
            cv.Optional(CONF_COMMAND_ON): cv.string_strict,
            cv.Optional(CONF_COMMAND_OFF): cv.string_strict,
        }
    ),
    _commands,
)


async def to_code(config):
    var = await switch.new_switch(config)
    bool_field_to_code(var, config)
    on = config.get(CONF_COMMAND_ON, config.get(CONF_COMMAND))
    off = config.get(CONF_COMMAND_OFF, config.get(CONF_COMMAND))
    cg.add(var.set_commands(on, off))
    hub = await register_listener(var, config)
    cg.add(var.set_parent(hub))
