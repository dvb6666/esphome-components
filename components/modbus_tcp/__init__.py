import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IP_ADDRESS,
    CONF_PORT,
    CONF_ADDRESS,
    CONF_DELAY,
    CONF_TIMEOUT,
)
from esphome.core import CORE

AUTO_LOAD = ["async_tcp"]
MULTI_CONF = True

CONF_RAW_DATA = "raw_data"

modbus_tcp_ns = cg.esphome_ns.namespace("modbus_tcp")
ModbusTcpComponent = modbus_tcp_ns.class_("ModbusTcpComponent", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusTcpComponent),
            cv.Required(CONF_IP_ADDRESS): cv.ipv4address, 
            cv.Optional(CONF_PORT, default=502): cv.port,
            cv.Optional(CONF_ADDRESS, default=0): cv.hex_uint8_t,
            cv.Optional(CONF_RAW_DATA, default=False): cv.boolean,
            cv.Optional(CONF_DELAY, default="100ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TIMEOUT, default="5s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], str(config[CONF_IP_ADDRESS]), config[CONF_PORT], config[CONF_ADDRESS])
    await cg.register_component(var, config)
    cg.add(var.set_delay_after_connect(config[CONF_DELAY]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    if config[CONF_RAW_DATA]:
        cg.add_define("USE_TCP_DEBUGGER")