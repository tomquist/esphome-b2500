import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_HAME_AT_ID, HameAtComponent, hame_at_ns

DEPENDENCIES = ["hame_at"]

CloudSwitch = hame_at_ns.class_("CloudSwitch", switch.Switch)

CONF_CLOUD = "cloud"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAME_AT_ID): cv.use_id(HameAtComponent),
        cv.Optional(CONF_CLOUD): switch.switch_schema(
            CloudSwitch,
            icon="mdi:cloud-sync",
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HAME_AT_ID])
    if conf := config.get(CONF_CLOUD):
        sw = await switch.new_switch(conf)
        await cg.register_parented(sw, parent)
