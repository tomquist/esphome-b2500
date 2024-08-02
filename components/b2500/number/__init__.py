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
    "DischargeThresholdNumber", number.Number, cg.Parented
)
DodNumber = b2500_ns.class_("DodNumber", number.Number)
TimerOutputPowerNumber = b2500_ns.class_(
    "TimerOutputPowerNumber", number.Number, cg.Parented
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

CONFIG_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
            cv.Required(CONF_B2500_GENERATION): cv.int_(1),
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
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            **{
                cv.Optional(f"timer{x + 1}_output_power"): cv.maybe_simple_value(
                    number.number_schema(
                        TimerOutputPowerNumber,
                        unit_of_measurement=UNIT_WATT,
                        entity_category=ENTITY_CATEGORY_CONFIG,
                    ),
                    key=CONF_NAME,
                )
                for x in range(3)
            },
        }
    ).extend(BASE_SCHEMA),
)

NUMBER_ATTRS = {
    CONF_DISCHARGE_THRESHOLD: {"min_value": 0, "max_value": 999, "step": 1},
    CONF_DOD: {"min_value": 0, "max_value": 100, "step": 1},
}


async def to_code(config):
    b2500_component = await cg.get_variable(config[CONF_B2500_ID])
    for switch_type in [
        CONF_DISCHARGE_THRESHOLD,
        CONF_DOD,
    ]:
        if conf := config.get(switch_type):
            number_attrs = NUMBER_ATTRS.get(switch_type, {})
            args = []
            btn = await number.new_number(conf, *args, **number_attrs)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            cg.add(getattr(b2500_component, f"set_{switch_type}_number")(btn))

    for x in range(3):
        switch_type = f"timer{x + 1}_output_power"
        if conf := config.get(switch_type):
            number_attrs = NUMBER_ATTRS.get(switch_type, {})
            args = [x]
            btn = await number.new_number(
                conf, *args, min_value=50, max_value=800, step=1
            )
            await cg.register_parented(btn, config[CONF_B2500_ID])
            cg.add(b2500_component.set_timer_output_power_number(x, btn))
