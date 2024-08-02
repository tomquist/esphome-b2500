import esphome.codegen as cg
from esphome.components import datetime
import esphome.config_validation as cv
from esphome.const import CONF_NAME

from .. import CONF_B2500_GENERATION, CONF_B2500_ID, B2500ComponentV2, b2500_ns

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

TimerStartTimeEntity = b2500_ns.class_(
    "TimerStartTimeEntity", datetime.TimeEntity, cg.Parented
)
TimerEndTimeEntity = b2500_ns.class_(
    "TimerEndTimeEntity", datetime.TimeEntity, cg.Parented
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
        cv.Required(CONF_B2500_GENERATION): cv.int_(2),
        **{
            cv.Optional(f"timer{x + 1}_start"): cv.maybe_simple_value(
                datetime.time_schema(
                    TimerStartTimeEntity,
                ),
                key=CONF_NAME,
            )
            for x in range(3)
        },
        **{
            cv.Optional(f"timer{x + 1}_end"): cv.maybe_simple_value(
                datetime.time_schema(
                    TimerEndTimeEntity,
                ),
                key=CONF_NAME,
            )
            for x in range(3)
        },
    }
)


async def to_code(config):
    b2500_component = await cg.get_variable(config[CONF_B2500_ID])
    for x in range(3):
        switch_type = f"timer{x + 1}_start"
        if conf := config.get(switch_type):
            btn = await datetime.new_datetime(conf, x)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            cg.add(b2500_component.set_timer_start_datetime(x, btn))
    for x in range(3):
        switch_type = f"timer{x + 1}_end"
        if conf := config.get(switch_type):
            btn = await datetime.new_datetime(conf, x)
            await cg.register_parented(btn, config[CONF_B2500_ID])
            cg.add(b2500_component.set_timer_end_datetime(x, btn))
