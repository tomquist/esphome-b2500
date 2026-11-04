import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from . import CONF_COMMAND, CONF_HAME_DEVICE_ID, CONF_MESSAGE, HameDevice, hame_device_ns

DEPENDENCIES = ["hame_device"]

HameDeviceButton = hame_device_ns.class_("HameDeviceButton", button.Button, cg.Parented.template(HameDevice))

CONFIG_SCHEMA = button.button_schema(HameDeviceButton).extend(
    {
        cv.GenerateID(CONF_HAME_DEVICE_ID): cv.use_id(HameDevice),
        cv.Required(CONF_COMMAND): cv.string_strict,
        # Message type whose keys the command uses and which is re-read after.
        cv.Optional(CONF_MESSAGE): cv.string_strict,
    }
)


async def to_code(config):
    var = await button.new_button(config)
    cg.add(var.set_command(config[CONF_COMMAND]))
    if CONF_MESSAGE in config:
        cg.add(var.set_message(config[CONF_MESSAGE]))
    hub = await cg.get_variable(config[CONF_HAME_DEVICE_ID])
    cg.add(var.set_parent(hub))
