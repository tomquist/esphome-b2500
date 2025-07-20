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

CONFIG_SCHEMA = cv.typed_schema(
    {
        1: cv.Schema(
            {
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
            }
        ).extend(BASE_SCHEMA),
        2: cv.Schema(
            {
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
            }
        ).extend(BASE_SCHEMA),
    },
    key=CONF_B2500_GENERATION,
    int=True,
)


async def to_code(config):
    if conf := config.get(CONF_CHARGE_MODE):
        btn = await select.new_select(conf, options=[])
        await cg.register_parented(btn, config[CONF_B2500_ID])
        await cg.register_component(btn, config)
