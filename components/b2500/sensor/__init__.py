import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    ICON_BATTERY,
    ICON_SIGNAL,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import (
    CONF_B2500_GENERATION,
    CONF_B2500_ID,
    B2500ComponentV1,
    B2500ComponentV2,
    b2500_ns,
)

DEPENDENCIES = ["b2500"]
CODEOWNERS = ["@tomquist"]

B2500Sensor = b2500_ns.class_("B2500Sensor", cg.Component)

CONF_SOC = "soc"
CONF_IN1_POWER = "in1_power"
CONF_IN2_POWER = "in2_power"
CONF_IN_TOTAL_POWER = "in_total_power"
CONF_OUT1_POWER = "out1_power"
CONF_OUT2_POWER = "out2_power"
CONF_OUT_TOTAL_POWER = "out_total_power"
CONF_CAPACITY = "capacity"
CONF_WIFI_RSSI = "wifi_rssi"
CONF_ADAPTIVE_POWER_OUT = "adaptive_power_out"
CONF_SMART_METER_READING = "smart_meter_reading"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_DAILY_TOTAL_BATTERY_CHARGE = "daily_total_battery_charge"
CONF_DAILY_TOTAL_BATTERY_DISCHARGE = "daily_total_battery_discharge"
CONF_DAILY_TOTAL_LOAD_CHARGE = "daily_total_load_charge"
CONF_DAILY_TOTAL_LOAD_DISCHARGE = "daily_total_load_discharge"

MARKERS: list[str] = [
    CONF_SOC,
    CONF_IN1_POWER,
    CONF_IN2_POWER,
    CONF_IN_TOTAL_POWER,
    CONF_OUT1_POWER,
    CONF_OUT2_POWER,
    CONF_OUT_TOTAL_POWER,
    CONF_CAPACITY,
    CONF_WIFI_RSSI,
    CONF_ADAPTIVE_POWER_OUT,
    CONF_SMART_METER_READING,
    CONF_TEMPERATURE_LOW,
    CONF_TEMPERATURE_HIGH,
    CONF_DAILY_TOTAL_BATTERY_CHARGE,
    CONF_DAILY_TOTAL_BATTERY_DISCHARGE,
    CONF_DAILY_TOTAL_LOAD_CHARGE,
    CONF_DAILY_TOTAL_LOAD_DISCHARGE,
]

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(B2500Sensor),
        cv.Optional(CONF_IN1_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon="mdi:solar-power",
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_POWER,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_IN2_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon="mdi:solar-power",
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_POWER,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_IN_TOTAL_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon="mdi:solar-power",
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_POWER,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_OUT1_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon="mdi:solar-power",
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_POWER,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_OUT2_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon="mdi:solar-power",
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_POWER,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_OUT_TOTAL_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon="mdi:solar-power",
                unit_of_measurement=UNIT_WATT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_POWER,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_SOC): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon=ICON_BATTERY,
                unit_of_measurement=UNIT_PERCENT,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_BATTERY,
                accuracy_decimals=1,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_CAPACITY): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon=ICON_BATTERY,
                unit_of_measurement=UNIT_WATT_HOURS,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_ENERGY_STORAGE,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_WIFI_RSSI): cv.maybe_simple_value(
            sensor.sensor_schema(
                icon=ICON_SIGNAL,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_TEMPERATURE_LOW): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_TEMPERATURE,
                accuracy_decimals=0,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_TEMPERATURE_HIGH): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_TEMPERATURE,
                accuracy_decimals=0,
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
        }
    ).extend(BASE_SCHEMA),
    cv.Schema(
        {
            cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            cv.Optional(CONF_ADAPTIVE_POWER_OUT): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT,
                    state_class=STATE_CLASS_MEASUREMENT,
                    device_class=DEVICE_CLASS_POWER,
                    accuracy_decimals=0,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SMART_METER_READING): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_VOLT_AMPS,
                    state_class=STATE_CLASS_MEASUREMENT,
                    accuracy_decimals=0,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_DAILY_TOTAL_BATTERY_CHARGE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT_HOURS,
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                    device_class=DEVICE_CLASS_ENERGY,
                    accuracy_decimals=0,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_DAILY_TOTAL_BATTERY_DISCHARGE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT_HOURS,
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                    device_class=DEVICE_CLASS_ENERGY,
                    accuracy_decimals=0,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_DAILY_TOTAL_LOAD_CHARGE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT_HOURS,
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                    device_class=DEVICE_CLASS_ENERGY,
                    accuracy_decimals=0,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_DAILY_TOTAL_LOAD_DISCHARGE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT_HOURS,
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                    device_class=DEVICE_CLASS_ENERGY,
                    accuracy_decimals=0,
                ),
                key=CONF_NAME,
            ),
        }
    ).extend(BASE_SCHEMA),
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_B2500_ID])
    bat = cg.new_Pvariable(config[CONF_ID], paren.get_state())

    for marker in MARKERS:
        if marker_config := config.get(marker):
            sens = await sensor.new_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_sensor")(sens))

    await cg.register_component(bat, config)
