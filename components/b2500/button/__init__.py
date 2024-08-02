import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_NAME

from .. import (
    CONF_B2500_GENERATION,
    CONF_B2500_ID,
    B2500ComponentBase,
    B2500ComponentV1,
    B2500ComponentV2,
    b2500_ns,
)

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

RebootButton = b2500_ns.class_("RebootButton", button.Button)
FactoryResetButton = b2500_ns.class_("FactoryResetButton", button.Button)

CONF_REBOOT = "reboot"
CONF_FACTORY_RESET = "factory_reset"

MARKERS = [CONF_REBOOT, CONF_FACTORY_RESET]

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentBase),
        cv.Optional(CONF_REBOOT): cv.maybe_simple_value(
            button.button_schema(RebootButton),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_FACTORY_RESET): cv.maybe_simple_value(
            button.button_schema(FactoryResetButton),
            key=CONF_NAME,
        ),
    }
)
CONFIG_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
            cv.Required(CONF_B2500_GENERATION): cv.int_(1),
        }
    ).extend(BASE_SCHEMA),
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
        }
    ).extend(BASE_SCHEMA),
)


async def to_code(config):
    for button_type in MARKERS:
        if conf := config.get(button_type):
            btn = await button.new_button(conf)
            await cg.register_parented(btn, config[CONF_B2500_ID])
