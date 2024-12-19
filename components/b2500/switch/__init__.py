import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_NAME, ENTITY_CATEGORY_CONFIG

from .. import (
    CONF_B2500_GENERATION,
    CONF_B2500_ID,
    B2500ComponentV1,
    B2500ComponentV2,
    b2500_ns,
)

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

OutActiveSwitch = b2500_ns.class_("OutActiveSwitch", switch.Switch)
TimerEnabledSwitch = b2500_ns.class_("TimerEnabledSwitch", switch.Switch)
AdaptiveModeSwitch = b2500_ns.class_("AdaptiveModeSwitch", switch.Switch)


CONF_ADAPTIVE_MODE = "adaptive_mode"

BASE_SCHEMA = cv.Schema({})

CONFIG_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
            cv.Required(CONF_B2500_GENERATION): cv.int_(1),
            **{
                cv.Optional(f"out{x + 1}"): cv.maybe_simple_value(
                    switch.switch_schema(
                        OutActiveSwitch,
                        entity_category=ENTITY_CATEGORY_CONFIG,
                    ),
                    key=CONF_NAME,
                )
                for x in range(2)
            },
        }
    ).extend(BASE_SCHEMA),
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            cv.Optional(CONF_ADAPTIVE_MODE): cv.maybe_simple_value(
                switch.switch_schema(
                    AdaptiveModeSwitch,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                key=CONF_NAME,
            ),
            **{
                cv.Optional(f"timer{x + 1}_enabled"): cv.maybe_simple_value(
                    switch.switch_schema(
                        TimerEnabledSwitch,
                        entity_category=ENTITY_CATEGORY_CONFIG,
                    ),
                    key=CONF_NAME,
                )
                for x in range(5)
            },
        }
    ).extend(BASE_SCHEMA),
)


async def to_code(config):
    b2500_component = await cg.get_variable(config[CONF_B2500_ID])
    if conf := config.get(CONF_ADAPTIVE_MODE):
        btn = await switch.new_switch(conf)
        await cg.register_parented(btn, config[CONF_B2500_ID])
        cg.add(b2500_component.set_adaptive_mode_switch(btn))
    for x in range(2):
        switch_type = f"out{x + 1}"
        if conf := config.get(switch_type):
            btn = await switch.new_switch(conf, x)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            cg.add(getattr(b2500_component, f"set_{switch_type}_switch")(btn))
    for x in range(5):
        switch_type = f"timer{x + 1}_enabled"
        if conf := config.get(switch_type):
            btn = await switch.new_switch(conf, x)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            cg.add(b2500_component.set_timer_enabled_switch(x, btn))
