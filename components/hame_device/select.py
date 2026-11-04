import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS

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

HameDeviceSelect = hame_device_ns.class_("HameDeviceSelect", select.Select, cg.Parented.template(HameDevice))

CONFIG_SCHEMA = (
    select.select_schema(HameDeviceSelect)
    .extend(ENTITY_SCHEMA)
    .extend(
        {
            cv.Required(CONF_KEY): cv.string_strict,
            # {value}: the device code of the chosen option.
            cv.Required(CONF_COMMAND): cv.string_strict,
            # device code -> label, in display order.
            cv.Required(CONF_OPTIONS): cv.All(cv.Schema({cv.string: cv.string}), cv.Length(min=1)),
        }
    )
)


async def to_code(config):
    options = config[CONF_OPTIONS]
    var = await select.new_select(config, options=list(options.values()))
    cg.add(var.set_key(config[CONF_KEY]))
    cg.add(var.set_command(config[CONF_COMMAND]))
    await keep_template_keys(config[CONF_HAME_DEVICE_ID], config[CONF_COMMAND])
    for code, label in options.items():
        cg.add(var.add_option_code(str(code), label))
    hub = await register_listener(var, config)
    cg.add(var.set_parent(hub))
