import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC

from .. import (
    CONF_B2500_GENERATION,
    B2500_COMPONENT_SCHEMA,
    CONF_B2500_ID,
    b2500_ns,
)

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

B2500TextSensor = b2500_ns.class_("B2500TextSensor", cg.Component)

CONF_FIRMWARE_VERSION = "firmware_version"
CONF_DEVICE_TYPE = "device_type"
CONF_DEVICE_ID = "device_id"
CONF_MAC_ADDRESS = "mac_address"
CONF_FC41D_VERSION = "fc41d_version"
CONF_WIFI_SSID = "wifi_ssid"
CONF_SCENE = "scene"
CONF_REGION = "region"
CONF_CELL_VOLTAGE = "cell_voltage"
CONF_LAST_RESPONSE = "last_response"

MARKERS: dict[str] = [
    CONF_FIRMWARE_VERSION,
    CONF_DEVICE_TYPE,
    CONF_DEVICE_ID,
    CONF_MAC_ADDRESS,
    CONF_FC41D_VERSION,
    CONF_WIFI_SSID,
    CONF_SCENE,
    CONF_REGION,
    CONF_CELL_VOLTAGE,
    CONF_LAST_RESPONSE,
]

CONFIG_SCHEMA = B2500_COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(B2500TextSensor),
        cv.Required(CONF_B2500_GENERATION): cv.int_range(1, 2),
    }
).extend(
    {
        cv.Optional(marker): cv.maybe_simple_value(
            text_sensor.text_sensor_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
            key=CONF_NAME,
        )
        for marker in MARKERS
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_B2500_ID])
    bat = cg.new_Pvariable(config[CONF_ID], paren.get_state())

    for marker in MARKERS:
        if marker_config := config.get(marker):
            var = await text_sensor.new_text_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_text_sensor")(var))

    await cg.register_component(bat, config)
