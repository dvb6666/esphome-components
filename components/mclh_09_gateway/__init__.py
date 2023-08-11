import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import myhomeiot_ble_host, esp32_ble_tracker
from esphome import const, automation
from esphome.const import (
    CONF_ID,
    CONF_INTERVAL,
    CONF_MAC_ADDRESS,
)

CODEOWNERS = ["@dvb666"]
AUTO_LOAD = ["sensor", "select", "myhomeiot_ble_client2"]
DEPENDENCIES = ["myhomeiot_ble_host"]

CONF_BLE_HOST = "ble_host"
CONF_ERROR_COUNTING = "error_counting"

mclh_09_gateway_ns = cg.esphome_ns.namespace("mclh_09_gateway")
Mclh09Gateway = mclh_09_gateway_ns.class_(
    "Mclh09Gateway", cg.Component
)
# Actions
Mclh09GatewayForceUpdateAction = mclh_09_gateway_ns.class_("Mclh09GatewayForceUpdateAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Mclh09Gateway),
            cv.GenerateID(CONF_BLE_HOST): cv.use_id(myhomeiot_ble_host.MyHomeIOT_BLEHost),
            cv.Required(CONF_MAC_ADDRESS): cv.ensure_list(cv.mac_address),
            cv.Optional(CONF_INTERVAL, default="60min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ERROR_COUNTING, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)
FORCE_UPDATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(Mclh09Gateway),
    }
)

def versiontuple(v):
    return tuple(map(int, (v.split("."))))

async def to_code(config):
    addr_list = []
    for it in config[CONF_MAC_ADDRESS]:
      addr_list.append(it.as_hex)
    var = cg.new_Pvariable(config[CONF_ID], addr_list, config[CONF_INTERVAL], config[CONF_ERROR_COUNTING])
    ble_host = await cg.get_variable(config[CONF_BLE_HOST])
    cg.add(var.set_ble_host(ble_host))
    await cg.register_component(var, config)

@automation.register_action(
    "mclh_09_gateway.force_update", Mclh09GatewayForceUpdateAction, FORCE_UPDATE_ACTION_SCHEMA
)
async def ble_write_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var
