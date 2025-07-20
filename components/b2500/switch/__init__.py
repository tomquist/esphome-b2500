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

OutActiveSwitch = b2500_ns.class_("OutActiveSwitch", switch.Switch, cg.Component, cg.Parented)
TimerEnabledSwitch = b2500_ns.class_("TimerEnabledSwitch", switch.Switch, cg.Component, cg.Parented)
AdaptiveModeSwitch = b2500_ns.class_("AdaptiveModeSwitch", switch.Switch, cg.Component, cg.Parented)


CONF_ADAPTIVE_MODE = "adaptive_mode"

BASE_SCHEMA = cv.Schema({})

CONFIG_SCHEMA = cv.typed_schema(
    {
        1: cv.Schema(
            {
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
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
        2: cv.Schema(
            {
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
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
    },
    key=CONF_B2500_GENERATION,
    int=True,
)


async def to_code(config):
    if conf := config.get(CONF_ADAPTIVE_MODE):
        btn = await switch.new_switch(conf)
        await cg.register_parented(btn, config[CONF_B2500_ID])
        await cg.register_component(btn, config)
    for x in range(2):
        switch_type = f"out{x + 1}"
        if conf := config.get(switch_type):
            btn = await switch.new_switch(conf, x)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            await cg.register_component(btn, config)
    for x in range(5):
        switch_type = f"timer{x + 1}_enabled"
        if conf := config.get(switch_type):
            btn = await switch.new_switch(conf, x)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            await cg.register_component(btn, config)
