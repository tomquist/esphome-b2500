import esphome.codegen as cg
from esphome.components import select
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

ChargeModeSelect = b2500_ns.class_(
    "ChargeModeSelect", select.Select, cg.Component, cg.Parented
)

CONF_CHARGE_MODE = "charge_mode"

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentBase),
        cv.Optional(CONF_CHARGE_MODE): cv.maybe_simple_value(
            select.select_schema(ChargeModeSelect),
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
    b2500_component = await cg.get_variable(config[CONF_B2500_ID])

    if conf := config.get(CONF_CHARGE_MODE):
        btn = await select.new_select(conf, options=[])
        await cg.register_parented(btn, config[CONF_B2500_ID])
        cg.add(getattr(b2500_component, f"set_{CONF_CHARGE_MODE}_select")(btn))
        await cg.register_component(btn, config)
