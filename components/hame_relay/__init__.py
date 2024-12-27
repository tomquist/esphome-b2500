import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.const import (
    CONF_ID,
    CONF_BROKER,
    CONF_PORT,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CLIENT_CERTIFICATE,
    CONF_CLIENT_CERTIFICATE_KEY,
)

CODEOWNERS = ["@tomquist"]
DEPENDENCIES = ["mqtt"]
CONF_DEVICES = "devices"
AUTO_LOAD = ["hame_relay", "mqtt"]
hame_relay_ns = cg.esphome_ns.namespace("hame_relay")
HameRelayComponent = hame_relay_ns.class_(
    "HameRelayComponent", cg.Component
)


DEVICE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.string,
        cv.Required("mac"): cv.string,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HameRelayComponent),
        cv.Required(CONF_DEVICES): cv.ensure_list(DEVICE_SCHEMA),
        cv.Required(CONF_BROKER): cv.string_strict,
        cv.Optional(CONF_PORT, default=8883): cv.port,
        cv.Required(CONF_CERTIFICATE_AUTHORITY): cv.All(
            cv.string, cv.only_with_esp_idf
        ),
        cv.Inclusive(CONF_CLIENT_CERTIFICATE, "cert-key-pair"): cv.All(
            cv.string, cv.only_on_esp32
        ),
        cv.Inclusive(CONF_CLIENT_CERTIFICATE_KEY, "cert-key-pair"): cv.All(
            cv.string, cv.only_on_esp32
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # Register as component first
    await cg.register_component(var, config)
    
    # Add device IDs
    # Add device IDs and MACs
    for device in config[CONF_DEVICES]:
        cg.add(var.add_device(device["id"], device["mac"]))

    cg.add(var.set_broker_address(config[CONF_BROKER]))
    cg.add(var.set_broker_port(config[CONF_PORT]))
    cg.add(var.set_ca_certificate(config[CONF_CERTIFICATE_AUTHORITY]))
    cg.add(var.set_cl_certificate(config[CONF_CLIENT_CERTIFICATE]))
    cg.add(var.set_cl_key(config[CONF_CLIENT_CERTIFICATE_KEY]))
    # prevent error -0x428e
    # See https://github.com/espressif/esp-idf/issues/139
    add_idf_sdkconfig_option("CONFIG_MBEDTLS_HARDWARE_MPI", False)


