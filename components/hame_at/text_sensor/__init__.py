import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_HAME_AT_ID, HameAtComponent

DEPENDENCIES = ["hame_at"]

CONF_CLOUD_STATUS = "cloud_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAME_AT_ID): cv.use_id(HameAtComponent),
        cv.Optional(CONF_CLOUD_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:cloud-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HAME_AT_ID])
    if conf := config.get(CONF_CLOUD_STATUS):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.set_cloud_status_sensor(sens))
