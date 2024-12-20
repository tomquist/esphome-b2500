from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import binary_sensor, ble_client, time
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATETIME,
    CONF_DAY,
    CONF_HOUR,
    CONF_ID,
    CONF_MINUTE,
    CONF_MONTH,
    CONF_NAME,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_SECOND,
    CONF_SSID,
    CONF_TIME_ID,
    CONF_UPDATE_INTERVAL,
    CONF_USERNAME,
    CONF_YEAR,
    ENTITY_CATEGORY_CONFIG,
)

CODEOWNERS = ["@tomquist"]
DEPENDENCIES = ["ble_client", "time"]
CONF_B2500_ID = "b2500_id"
CONF_B2500_GENERATION = "generation"
CONF_HOST = "host"
CONF_SSL = "ssl"
CONF_CHARGE_MODE = "charge_mode"

AUTO_LOAD = ["b2500", "binary_sensor", "switch"]
MULTI_CONF = 3

b2500_ns = cg.esphome_ns.namespace("b2500")

B2500ComponentBase = b2500_ns.class_(
    "B2500ComponentBase", ble_client.BLEClientNode, cg.PollingComponent
)
B2500ComponentV1 = b2500_ns.class_("B2500ComponentV1", B2500ComponentBase)
B2500ComponentV2 = b2500_ns.class_("B2500ComponentV2", B2500ComponentBase)

SetWifiAction = b2500_ns.class_("SetWifiAction", automation.Action)
SetMqttAction = b2500_ns.class_("SetMqttAction", automation.Action)
ResetMqttAction = b2500_ns.class_("ResetMqttAction", automation.Action)
SetDatetimeAction = b2500_ns.class_("SetDatetimeAction", automation.Action)
RebootAction = b2500_ns.class_("RebootAction", automation.Action)
FactoryResetAction = b2500_ns.class_("FactoryResetAction", automation.Action)
SetDodAction = b2500_ns.class_("SetDodAction", automation.Action)
SetChargeModeAction = b2500_ns.class_("SetChargeModeAction", automation.Action)
SetOutActiveAction = b2500_ns.class_("SetOutActiveAction", automation.Action)
SetDischargeThresholdAction = b2500_ns.class_(
    "SetDischargeThresholdAction", automation.Action
)
SetTimerAction = b2500_ns.class_("SetTimerAction", automation.Action)
SetAdaptiveModeEnabledAction = b2500_ns.class_("SetAdaptiveModeEnabledAction", automation.Action)

B2500_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_B2500_ID): cv.use_id(B2500ComponentBase),
    }
)

BASE_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.update_interval,
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("10s"))
)

CONFIG_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(B2500ComponentV1),
            cv.Required(CONF_B2500_GENERATION): cv.int_(1),
        }
    ).extend(BASE_SCHEMA),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            cv.Optional("timer"): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                key=CONF_NAME,
            ),
        }
    ).extend(BASE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    if time_id_config := config.get(CONF_TIME_ID):
        time_id = await cg.get_variable(time_id_config)
        cg.add(var.set_time(time_id))

    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)


B2500_BASE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(B2500ComponentBase),
    }
)


@automation.register_action(
    "b2500.set_wifi",
    SetWifiAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentBase),
            cv.Required(CONF_SSID): cv.templatable(cv.string),
            cv.Required(CONF_PASSWORD): cv.templatable(cv.string),
        }
    ),
)
async def b2500_set_wifi(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_ssid(await cg.templatable(config[CONF_SSID], args, cg.std_string)))
    cg.add(
        var.set_password(
            await cg.templatable(config[CONF_PASSWORD], args, cg.std_string)
        )
    )
    return var


@automation.register_action(
    "b2500.set_mqtt",
    SetMqttAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentBase),
            cv.Optional(CONF_SSL, default=False): cv.boolean,
            cv.Required(CONF_HOST): cv.templatable(cv.string),
            cv.Required(CONF_PORT): cv.templatable(cv.int_),
            cv.Required(CONF_USERNAME): cv.templatable(cv.string),
            cv.Required(CONF_PASSWORD): cv.templatable(cv.string),
        }
    ),
)
async def b2500_set_mqtt(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_ssl(config[CONF_SSL]))
    cg.add(var.set_host(await cg.templatable(config[CONF_HOST], args, cg.std_string)))
    cg.add(var.set_port(await cg.templatable(config[CONF_PORT], args, cg.int_)))
    cg.add(
        var.set_username(
            await cg.templatable(config[CONF_USERNAME], args, cg.std_string)
        )
    )
    cg.add(
        var.set_password(
            await cg.templatable(config[CONF_PASSWORD], args, cg.std_string)
        )
    )
    return var

@automation.register_action(
    "b2500.reset_mqtt",
    ResetMqttAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentBase),
        }
    ),
)
async def b2500_reset_mqtt(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "b2500.set_datetime",
    SetDatetimeAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentBase),
            cv.Required(CONF_DATETIME): cv.Any(
                cv.returning_lambda, cv.date_time(date=True, time=True)
            ),
        },
    ),
)
async def b2500_set_datetime(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    datetime_config = config[CONF_DATETIME]
    if cg.is_template(datetime_config):
        template_ = await cg.templatable(datetime_config, [], cg.ESPTime)
        cg.add(action_var.set_datetime(template_))
    else:
        datetime_struct = cg.StructInitializer(
            cg.ESPTime,
            ("second", datetime_config[CONF_SECOND]),
            ("minute", datetime_config[CONF_MINUTE]),
            ("hour", datetime_config[CONF_HOUR]),
            ("day_of_month", datetime_config[CONF_DAY]),
            ("month", datetime_config[CONF_MONTH]),
            ("year", datetime_config[CONF_YEAR]),
        )
        cg.add(action_var.set_datetime(datetime_struct))
    return action_var


@automation.register_action(
    "b2500.reboot",
    RebootAction,
    B2500_BASE_ACTION_SCHEMA,
)
async def b2500_reboot(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "b2500.factory_reset",
    FactoryResetAction,
    B2500_BASE_ACTION_SCHEMA,
)
async def b2500_factory_reset(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "b2500.set_dod",
    SetDodAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentBase),
            cv.Required("dod"): cv.int_,
        },
    ),
)
async def b2500_set_dod(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    cg.add(action_var.set_dod(config["dod"]))
    return action_var


@automation.register_action(
    "b2500.set_charge_mode",
    SetChargeModeAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentV1),
            cv.Required(CONF_B2500_GENERATION): cv.int_(1),
            cv.Required(CONF_CHARGE_MODE): cv.enum(
                {
                    "load_first": "LoadFirst",
                    "pv2_passthrough": "PV2Passthrough",
                }
            ),
        },
    ),
)
@automation.register_action(
    "b2500.set_charge_mode",
    SetChargeModeAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            cv.Required(CONF_CHARGE_MODE): cv.enum(
                {
                    "load_first": "LoadFirst",
                    "simultaneous_charge_and_discharge": "SimultaneousChargeAndDischarge",
                }
            ),
        },
    ),
)
async def b2500_set_charge_mode(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    cg.add(action_var.set_charge_mode(config["charge_mode"]))
    return action_var


@automation.register_action(
    "b2500.set_out_active",
    SetOutActiveAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentV1),
            cv.Required("out"): cv.int_,
            cv.Required("active"): cv.boolean,
        },
    ),
)
async def b2500_set_out_active(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    cg.add(action_var.set_out(config["out"]))
    cg.add(action_var.set_active(config["active"]))
    return action_var


@automation.register_action(
    "b2500.set_discharge_threshold",
    SetDischargeThresholdAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentV1),
            cv.Required("threshold"): cv.int_,
        },
    ),
)
async def b2500_set_discharge_threshold(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    cg.add(action_var.set_threshold(config["threshold"]))
    return action_var


@automation.register_action(
    "b2500.set_timer",
    SetTimerAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            cv.Required("timer"): cv.templatable(cv.int_range(0, 4)),
            cv.Optional("enabled"): cv.templatable(cv.boolean),
            cv.Optional("output_power"): cv.templatable(cv.int_),
            cv.Optional("start_hour"): cv.templatable(cv.int_),
            cv.Optional("start_minute"): cv.templatable(cv.int_),
            cv.Optional("end_hour"): cv.templatable(cv.int_),
            cv.Optional("end_minute"): cv.templatable(cv.int_),
        },
    ).add_extra(
        cv.has_at_least_one_key(
            "enabled",
            "output_power",
            "start_hour",
            "start_minute",
            "end_hour",
            "end_minute",
        )
    )
)
async def b2500_set_timer(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    template_ = await cg.templatable(config["timer"], args, cg.std_string)
    cg.add(action_var.set_timer(template_))
    if "enabled" in config:
        template_ = await cg.templatable(config["enabled"], args, cg.optional.template(bool))
        cg.add(action_var.set_enabled(template_))
    if "output_power" in config:
        template_ = await cg.templatable(config["output_power"], args, cg.optional.template(cg.int_))
        cg.add(action_var.set_output_power(template_))
    if "start_hour" in config:
        template_ = await cg.templatable(config["start_hour"], args, cg.optional.template(cg.int_))
        cg.add(action_var.set_start_hour(template_))
    if "start_minute" in config:
        template_ = await cg.templatable(config["start_minute"], args, cg.optional.template(cg.int_))
        cg.add(action_var.set_start_minute(template_))
    if "end_hour" in config:
        template_ = await cg.templatable(config["end_hour"], args, cg.optional.template(cg.int_))
        cg.add(action_var.set_end_hour(template_))
    if "end_minute" in config:
        template_ = await cg.templatable(config["end_minute"], args, cg.optional.template(cg.int_))
        cg.add(action_var.set_end_minute(template_))
    return action_var


@automation.register_action(
    "b2500.set_adaptive_mode_enabled",
    SetAdaptiveModeEnabledAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(B2500ComponentV2),
            cv.Required(CONF_B2500_GENERATION): cv.int_(2),
            cv.Required("enabled"): cv.templatable(cv.boolean),
        },
    ),
)
async def b2500_set_adaptive_mode_enabled(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    template_ = await cg.templatable(config["enabled"], args, bool)
    cg.add(action_var.set_enabled(template_))
    return action_var
