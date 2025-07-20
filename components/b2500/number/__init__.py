import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_NAME, ENTITY_CATEGORY_CONFIG, UNIT_PERCENT, UNIT_WATT

from .. import (
    CONF_B2500_GENERATION,
    CONF_B2500_ID,
    B2500ComponentV1,
    B2500ComponentV2,
    b2500_ns,
)

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

CONF_DISCHARGE_THRESHOLD = "discharge_threshold"
CONF_DOD = "dod"

DischargeThresholdNumber = b2500_ns.class_(
    "DischargeThresholdNumber", number.Number, cg.Component, cg.Parented
)
DodNumber = b2500_ns.class_(
    "DodNumber", number.Number, cg.Component, cg.Parented
)
TimerOutputPowerNumber = b2500_ns.class_(
    "TimerOutputPowerNumber", number.Number, cg.Component, cg.Parented
)

BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DOD): cv.maybe_simple_value(
            number.number_schema(
                DodNumber,
                unit_of_measurement=UNIT_PERCENT,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            key=CONF_NAME,
        ),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        1: cv.Schema(
            {
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
                cv.Optional(CONF_DISCHARGE_THRESHOLD): cv.maybe_simple_value(
                    number.number_schema(
                        DischargeThresholdNumber,
                        unit_of_measurement=UNIT_WATT,
                        entity_category=ENTITY_CATEGORY_CONFIG,
                    ),
                    key=CONF_NAME,
                ),
            }
        ).extend(BASE_SCHEMA),
        2: cv.Schema(
            {
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
                **{
                    cv.Optional(f"timer{x + 1}_output_power"): cv.maybe_simple_value(
                        number.number_schema(
                            TimerOutputPowerNumber,
                            unit_of_measurement=UNIT_WATT,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                        key=CONF_NAME,
                    )
                    for x in range(5)
                },
            }
        ).extend(BASE_SCHEMA),
    },
    key=CONF_B2500_GENERATION,
    int=True,
)

NUMBER_ATTRS = {
    CONF_DISCHARGE_THRESHOLD: {"min_value": 0, "max_value": 999, "step": 1},
    CONF_DOD: {"min_value": 0, "max_value": 100, "step": 1},
}


async def to_code(config):
    for switch_type in [
        CONF_DISCHARGE_THRESHOLD,
        CONF_DOD,
    ]:
        if conf := config.get(switch_type):
            number_attrs = NUMBER_ATTRS.get(switch_type, {})
            args = []
            btn = await number.new_number(conf, *args, **number_attrs)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            await cg.register_component(btn, config)

    for x in range(5):
        switch_type = f"timer{x + 1}_output_power"
        if conf := config.get(switch_type):
            number_attrs = NUMBER_ATTRS.get(switch_type, {})
            args = [x]
            btn = await number.new_number(
                conf, *args, min_value=50, max_value=800, step=1
            )
            await cg.register_parented(btn, config[CONF_B2500_ID])
            await cg.register_component(btn, config)
