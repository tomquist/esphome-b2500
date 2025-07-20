import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

from .. import (
    CONF_B2500_GENERATION,
    CONF_B2500_ID,
    B2500ComponentV1,
    B2500ComponentV2,
    b2500_ns,
)

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

B2500BinarySensorBase = b2500_ns.class_("B2500BinarySensorBase", cg.Component)
B2500BinarySensorV1 = b2500_ns.class_("B2500BinarySensorV1", B2500BinarySensorBase)
B2500BinarySensorV2 = b2500_ns.class_("B2500BinarySensorV2", B2500BinarySensorBase)

CONF_WIFI_CONNECTED = "wifi_connected"
CONF_MQTT_CONNECTED = "mqtt_connected"
CONF_IN1_ACTIVE = "in1_active"
CONF_IN2_ACTIVE = "in2_active"
CONF_IN1_TRANSPARENT = "in1_transparent"
CONF_IN2_TRANSPARENT = "in2_transparent"
CONF_OUT1_ACTIVE = "out1_active"
CONF_OUT2_ACTIVE = "out2_active"
CONF_EXTERN1_CONNECTED = "extern1_connected"
CONF_EXTERN2_CONNECTED = "extern2_connected"
CONF_SMART_METER_CONNECTED = "smart_meter_connected"

BASE_MARKERS: list[str] = [
    CONF_WIFI_CONNECTED,
    CONF_MQTT_CONNECTED,
    CONF_IN1_ACTIVE,
    CONF_IN2_ACTIVE,
    CONF_IN1_TRANSPARENT,
    CONF_IN2_TRANSPARENT,
    CONF_OUT1_ACTIVE,
    CONF_OUT2_ACTIVE,
    CONF_EXTERN1_CONNECTED,
    CONF_EXTERN2_CONNECTED,
]
V1_MARKERS: list[str] = []
V2_MARKERS: list[str] = [CONF_SMART_METER_CONNECTED]
ALL_MARKERS: list[str] = BASE_MARKERS + V1_MARKERS + V2_MARKERS

BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(marker): cv.maybe_simple_value(
            binary_sensor.binary_sensor_schema(),
            key=CONF_NAME,
        )
        for marker in BASE_MARKERS
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        1: cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(B2500BinarySensorV1),
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV1),
                **{
                    cv.Optional(marker): cv.maybe_simple_value(
                        binary_sensor.binary_sensor_schema(),
                        key=CONF_NAME,
                    )
                    for marker in V1_MARKERS
                },
            }
        ).extend(BASE_SCHEMA),
        2: cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(B2500BinarySensorV2),
                cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
                **{
                    cv.Optional(marker): cv.maybe_simple_value(
                        binary_sensor.binary_sensor_schema(),
                        key=CONF_NAME,
                    )
                    for marker in V2_MARKERS
                },
            }
        ).extend(BASE_SCHEMA),
    },
    key=CONF_B2500_GENERATION,
    int=True,
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_B2500_ID])
    bat = cg.new_Pvariable(config[CONF_ID], paren.get_state())

    for marker in ALL_MARKERS:
        if marker_config := config.get(marker):
            sens = await binary_sensor.new_binary_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_binary_sensor")(sens))

    await cg.register_component(bat, config)
