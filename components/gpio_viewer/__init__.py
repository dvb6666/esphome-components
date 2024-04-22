import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import const, automation
from esphome.components import wifi
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_WIFI,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@dvb666"]
DEPENDENCIES = ["wifi", "esp32"]
MULTI_CONF = False

CONF_SAMPLING_INTERVAL = "sampling_interval"

gpio_viewer_ns = cg.esphome_ns.namespace("gpio_viewer")
GPIOViewerProxy = gpio_viewer_ns.class_(
    "GPIOViewerProxy", cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GPIOViewerProxy),
            cv.GenerateID(CONF_WIFI): cv.use_id(wifi.WiFiComponent),
            cv.Optional(CONF_PORT, default=8080): cv.port,
            cv.Optional(CONF_SAMPLING_INTERVAL, default=100): cv.uint32_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    wifi_component = await cg.get_variable(config[CONF_WIFI])
    var = cg.new_Pvariable(config[CONF_ID], wifi_component, config[CONF_PORT], config[CONF_SAMPLING_INTERVAL])
    await cg.register_component(var, config)
