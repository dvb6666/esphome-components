import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import myhomeiot_ble_host, esp32_ble_tracker
from esphome import const, automation
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_SERVICES,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
    CONF_ON_CONNECT,
    CONF_ON_VALUE,
    CONF_VALUE,
    CONF_DELAY,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@dvb666"]
DEPENDENCIES = ["myhomeiot_ble_host"]
MULTI_CONF = True

CONF_BLE_HOST = "ble_host"
CONF_ON_ERROR = "on_error"
CONF_CHARACTERISTIC_UUID = "characteristic_uuid"
CONF_DESCRIPTOR_UUID = "descriptor_uuid"
CONF_NOTIFY = "notify"
CONF_SKIP_EMPTY = "skip_empty"

myhomeiot_ble_client2_ns = cg.esphome_ns.namespace("myhomeiot_ble_client2")
MyHomeIOT_BLEClient2 = myhomeiot_ble_client2_ns.class_(
    "MyHomeIOT_BLEClient2", cg.Component
)
MyHomeIOT_BLEClient2ConstRef = MyHomeIOT_BLEClient2.operator("ref").operator("const")

MyHomeIOT_BLEClientService = myhomeiot_ble_client2_ns.class_(
    "MyHomeIOT_BLEClientService", cg.EntityBase
)
BoolRef = cg.bool_.operator("ref")

# Triggers
MyHomeIOT_BLEClientConnectTrigger = myhomeiot_ble_client2_ns.class_(
    "MyHomeIOT_BLEClientConnectTrigger",
    automation.Trigger.template(cg.int_),
)
MyHomeIOT_BLEClientValueTrigger = myhomeiot_ble_client2_ns.class_(
    "MyHomeIOT_BLEClientValueTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8), cg.int_),
)
MyHomeIOT_BLEClientErrorTrigger = myhomeiot_ble_client2_ns.class_(
    "MyHomeIOT_BLEClientErrorTrigger",
    automation.Trigger.template(cg.uint32),
)
# Actions
MyHomeIOT_BLEClientForceUpdateAction = myhomeiot_ble_client2_ns.class_("MyHomeIOT_BLEClientForceUpdateAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MyHomeIOT_BLEClient2),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SERVICES): cv.ensure_list(
                {
                    cv.GenerateID(): cv.declare_id(MyHomeIOT_BLEClientService),
                    cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
                    cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
                    cv.Optional(CONF_DESCRIPTOR_UUID): esp32_ble_tracker.bt_uuid,
                    cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
                    cv.Optional(CONF_DELAY): cv.templatable(cv.positive_time_period_milliseconds),
                    cv.Optional(CONF_SKIP_EMPTY, default=False): cv.boolean,
                    cv.Optional(CONF_VALUE): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
                }
            ),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MyHomeIOT_BLEClientConnectTrigger),
                }
            ),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MyHomeIOT_BLEClientValueTrigger),
                }
            ),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MyHomeIOT_BLEClientErrorTrigger),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60min"))
    .extend(myhomeiot_ble_host.BLE_CLIENT_SCHEMA)
)
FORCE_UPDATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(MyHomeIOT_BLEClient2),
    }
)


def versiontuple(v):
    return tuple(map(int, (v.split("."))))

async def to_code(config):
    if CONF_MAC_ADDRESS not in config:
        _LOGGER.warning('No MAC-address. Component will be ignored')
        return

    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await myhomeiot_ble_host.register_ble_client(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    for service in config[CONF_SERVICES]:
        srv = cg.new_Pvariable(service[CONF_ID])
        if len(service[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
          cg.add(srv.set_service_uuid16(esp32_ble_tracker.as_hex(service[CONF_SERVICE_UUID])))
        elif len(service[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid32_format):
          cg.add(srv.set_service_uuid32(esp32_ble_tracker.as_hex(service[CONF_SERVICE_UUID])))
        elif len(service[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid128_format):
          uuid128 = esp32_ble_tracker.as_reversed_hex_array(service[CONF_SERVICE_UUID]) if reversed else esp32_ble_tracker.as_hex_array(service[CONF_SERVICE_UUID])
          cg.add(srv.set_service_uuid128(uuid128))

        if len(service[CONF_CHARACTERISTIC_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
          cg.add(srv.set_char_uuid16(esp32_ble_tracker.as_hex(service[CONF_CHARACTERISTIC_UUID])))
        elif len(service[CONF_CHARACTERISTIC_UUID]) == len(esp32_ble_tracker.bt_uuid32_format):
          cg.add(srv.set_char_uuid32(esp32_ble_tracker.as_hex(service[CONF_CHARACTERISTIC_UUID])))
        elif len(service[CONF_CHARACTERISTIC_UUID]) == len(esp32_ble_tracker.bt_uuid128_format):
          uuid128 = esp32_ble_tracker.as_reversed_hex_array(service[CONF_CHARACTERISTIC_UUID]) if reversed else esp32_ble_tracker.as_hex_array(service[CONF_CHARACTERISTIC_UUID])
          cg.add(srv.set_char_uuid128(uuid128))

        if uuid := service.get(CONF_DESCRIPTOR_UUID):
          if len(uuid) == len(esp32_ble_tracker.bt_uuid16_format):
            cg.add(srv.set_descr_uuid16(esp32_ble_tracker.as_hex(uuid)))
          elif len(uuid) == len(esp32_ble_tracker.bt_uuid32_format):
            cg.add(srv.set_descr_uuid32(esp32_ble_tracker.as_hex(uuid)))
          elif len(uuid) == len(esp32_ble_tracker.bt_uuid128_format):
            uuid128 = esp32_ble_tracker.as_reversed_hex_array(uuid) if reversed else esp32_ble_tracker.as_hex_array(uuid)
            cg.add(srv.set_descr_uuid128(uuid128))

        if value := service.get(CONF_VALUE):
          if cg.is_template(value):
            templ = await cg.templatable(value, [], cg.std_vector.template(cg.uint8))
            cg.add(srv.set_value_template(templ))
          else:
            cg.add(srv.set_value_simple(value))

        if value := service.get(CONF_DELAY):
          if cg.is_template(value):
            templ = await cg.templatable(value, [], cg.uint32)
            cg.add(srv.set_delay_template(templ))
          else:
            cg.add(srv.set_delay_simple(value))

        if service[CONF_NOTIFY]:
          cg.add(srv.set_notify())
        if service[CONF_SKIP_EMPTY]:
          cg.add(srv.set_skip_empty())

        cg.add(var.add_service(srv))

    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.int_, "rssi"), (MyHomeIOT_BLEClient2ConstRef, "xthis")], conf)
    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_vector.template(cg.uint8), "x"), (cg.int_, "service"), (BoolRef, "stop_processing"), (MyHomeIOT_BLEClient2ConstRef, "xthis")], conf)
    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "error_count"), (MyHomeIOT_BLEClient2ConstRef, "xthis")], conf)

@automation.register_action(
    "myhomeiot_ble_client2.force_update", MyHomeIOT_BLEClientForceUpdateAction, FORCE_UPDATE_ACTION_SCHEMA
)
async def ble_write_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var
