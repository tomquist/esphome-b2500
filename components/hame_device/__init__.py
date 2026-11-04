"""A Hame/Marstek device described in YAML (key=value status, cd= commands).

The hub parses the device's status messages and sends commands; what each key
means is configured with the entity platforms of this component (sensor,
binary_sensor, text_sensor, number, select, switch, button). Device profiles
in examples/fc41d/devices/ bundle those as YAML packages per device family.
"""

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERVAL, CONF_VALUE

CODEOWNERS = ["@tomquist"]
MULTI_CONF = True

CONF_HAME_DEVICE_ID = "hame_device_id"
CONF_HAME_AT_ID = "hame_at_id"
CONF_POLL = "poll"
CONF_COMMAND = "command"
CONF_MESSAGES = "messages"
CONF_MESSAGE = "message"
CONF_NAME_ = "name"
CONF_REQUIRED_KEYS = "required_keys"
CONF_EXCLUDED_KEYS = "excluded_keys"
CONF_KEY_COUNT = "key_count"
CONF_POLL_AFTER_COMMAND = "poll_after_command"
CONF_TOPIC_FILTER = "topic_filter"
CONF_ON_MESSAGE = "on_message"
CONF_ON_COMMAND = "on_command"

hame_device_ns = cg.esphome_ns.namespace("hame_device")
HameDevice = hame_device_ns.class_("HameDevice", cg.Component)
HameAtComponent = cg.esphome_ns.namespace("hame_at").class_("HameAtComponent", cg.Component)
SendCommandAction = hame_device_ns.class_("SendCommandAction", automation.Action)
PollAction = hame_device_ns.class_("PollAction", automation.Action)

MESSAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME_): cv.string_strict,
        # Recognised by: all of required_keys present, none of excluded_keys,
        # and exactly key_count keys (if set).
        cv.Optional(CONF_REQUIRED_KEYS, default=[]): cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_EXCLUDED_KEYS, default=[]): cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_KEY_COUNT, default=0): cv.int_range(min=0, max=255),
        # Requested with this command (none: the device sends it by itself).
        cv.Optional(CONF_POLL): cv.string_strict,
        cv.Optional(CONF_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
    }
)


def _unique_messages(messages):
    names = [m[CONF_NAME_] for m in messages]
    for n in names:
        if names.count(n) > 1:
            raise cv.Invalid(f"Message type '{n}' is defined twice")
    return messages

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HameDevice),
        # Wire to the FC41D emulator: the battery MCU's MQTT session.
        cv.Optional(CONF_HAME_AT_ID): cv.use_id(HameAtComponent),
        # Message types, tried in order; the first is the default for
        # entities. Device profiles add theirs (with !extend). None: every
        # message is one type.
        cv.Optional(CONF_MESSAGES, default=[]): cv.All(cv.ensure_list(MESSAGE_SCHEMA), _unique_messages),
        cv.Optional(CONF_POLL_AFTER_COMMAND, default="1500ms"): cv.positive_time_period_milliseconds,
        # Only status from topics containing this (hame_at default: "/device/").
        cv.Optional(CONF_TOPIC_FILTER): cv.string,
        # Every status message (`payload`), and every command sent.
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({}),
        cv.Optional(CONF_ON_COMMAND): automation.validate_automation({}),
    }
).extend(cv.COMPONENT_SCHEMA)


# Shared by the entity platforms.
ENTITY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAME_DEVICE_ID): cv.use_id(HameDevice),
        # The message type the entity reads (default: the hub's first).
        cv.Optional(CONF_MESSAGE): cv.string_strict,
    }
)


async def register_listener(var, config):
    hub = await cg.get_variable(config[CONF_HAME_DEVICE_ID])
    cg.add(hub.add_listener(var))
    if CONF_MESSAGE in config:
        cg.add(var.set_message(config[CONF_MESSAGE]))
    return hub


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_TOPIC_FILTER in config:
        cg.add(var.set_topic_filter(config[CONF_TOPIC_FILTER]))
    if CONF_HAME_AT_ID in config:
        at = await cg.get_variable(config[CONF_HAME_AT_ID])
        cg.add(var.set_hame_at(at))
    for m in config[CONF_MESSAGES]:
        cg.add(
            var.add_message(
                m[CONF_NAME_],
                m[CONF_REQUIRED_KEYS],
                m[CONF_EXCLUDED_KEYS],
                m[CONF_KEY_COUNT],
                m.get(CONF_POLL, ""),
                m[CONF_INTERVAL],
            )
        )
    cg.add(var.set_poll_after_command(config[CONF_POLL_AFTER_COMMAND]))
    for conf in config.get(CONF_ON_MESSAGE, []):
        await automation.build_automation(var.get_message_trigger(), [(cg.std_string, "payload")], conf)
    for conf in config.get(CONF_ON_COMMAND, []):
        await automation.build_automation(var.get_command_trigger(), [(cg.std_string, "payload")], conf)
    cg.add_define("USE_HAME_DEVICE")


@automation.register_action(
    "hame_device.send_command",
    SendCommandAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HameDevice),
            cv.Required(CONF_COMMAND): cv.templatable(cv.string),
            cv.Optional(CONF_VALUE): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def send_command_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cmd = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(cmd))
    if CONF_VALUE in config:
        val = await cg.templatable(config[CONF_VALUE], args, cg.std_string)
        cg.add(var.set_value(val))
    return var


@automation.register_action(
    "hame_device.poll",
    PollAction,
    cv.Schema({cv.GenerateID(): cv.use_id(HameDevice)}),
    synchronous=True,
)
async def poll_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


# --- field options shared by the platforms ---------------------------------
CONF_KEY = "key"
CONF_BIT = "bit"
CONF_BITS = "bits"
CONF_VALID_MIN = "valid_min"
CONF_VALID_MAX = "valid_max"
CONF_MULTIPLY = "multiply"
CONF_ON_VALUES = "on_values"
CONF_AGGREGATE = "aggregate"
CONF_IGNORE_ZERO = "ignore_zero"

NumberField = hame_device_ns.struct("NumberField")
AGGREGATES = {
    "sum": NumberField.enum("SUM"),
    "min": NumberField.enum("MIN"),
    "max": NumberField.enum("MAX"),
    "average": NumberField.enum("AVERAGE"),
    "spread": NumberField.enum("SPREAD"),
}

key_list = cv.All(cv.ensure_list(cv.string_strict), cv.Length(min=1))

NUMBER_FIELD_SCHEMA = cv.Schema(
    {
        # One key, or several whose values are summed.
        cv.Required(CONF_KEY): key_list,
        cv.Optional(CONF_BIT): cv.int_range(min=0, max=62),
        cv.Optional(CONF_BITS, default=1): cv.int_range(min=1, max=32),
        # Values outside are ignored (the devices' "no reading" sentinels).
        cv.Optional(CONF_VALID_MIN): cv.float_,
        cv.Optional(CONF_VALID_MAX): cv.float_,
        cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
        # How several keys combine: sum (all keys needed), or min/max/
        # average/spread over the keys present.
        cv.Optional(CONF_AGGREGATE, default="sum"): cv.enum(AGGREGATES, lower=True),
        cv.Optional(CONF_IGNORE_ZERO, default=False): cv.boolean,
    }
)

BOOL_FIELD_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_KEY): cv.string_strict,
        cv.Optional(CONF_BIT): cv.int_range(min=0, max=62),
        # Raw values meaning "on"; default: any non-zero number.
        cv.Optional(CONF_ON_VALUES): cv.ensure_list(cv.string),
    }
)


def number_field_to_code(var, config):
    field = var.field()
    for key in config[CONF_KEY]:
        cg.add(field.add_key(key))
    if CONF_BIT in config:
        cg.add(field.set_bit(config[CONF_BIT], config[CONF_BITS]))
    if CONF_VALID_MIN in config:
        cg.add(field.set_valid_min(config[CONF_VALID_MIN]))
    if CONF_VALID_MAX in config:
        cg.add(field.set_valid_max(config[CONF_VALID_MAX]))
    if config[CONF_MULTIPLY] != 1.0:
        cg.add(field.set_multiply(config[CONF_MULTIPLY]))
    if config[CONF_AGGREGATE] != "sum" or config[CONF_IGNORE_ZERO]:
        cg.add(field.set_aggregate(AGGREGATES[config[CONF_AGGREGATE]], config[CONF_IGNORE_ZERO]))


def bool_field_to_code(var, config):
    field = var.field()
    cg.add(field.set_key(config[CONF_KEY]))
    if CONF_BIT in config:
        cg.add(field.set_bit(config[CONF_BIT]))
    for v in config.get(CONF_ON_VALUES, []):
        cg.add(field.add_on_value(v))
