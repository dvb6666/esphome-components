import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, switch
from esphome.components import modbus_tcp
from esphome.const import (
    CONF_ID,
    CONF_DEVICE_ID,
    CONF_NAME,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_SWITCH,
    DEVICE_CLASS_WATER,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CUBIC_METER,
)

DEPENDENCIES = ["modbus_tcp"]
AUTO_LOAD = ["binary_sensor", "sensor", "switch"]
MULTI_CONF = True

MAX_ZONES = 2
MAX_WIRE_LINE_LEAK_SENSORS = 4
MAX_WATER_COUNTERS = 8

CONF_MODBUS_ID = "modbus_id"
CONF_CONNECTION_STATUS = "connection_status"
CONF_ALERT_ZONE = "alert_zone"
CONF_VALVE_ZONE = "valve_zone"
CONF_ALERT_ZONE_ARRAY = list(map(lambda x: CONF_ALERT_ZONE + "_" + str(1 + x), range(0, MAX_ZONES)))
CONF_VALVE_ZONE_ARRAY = list(map(lambda x: CONF_VALVE_ZONE + "_" + str(1 + x), range(0, MAX_ZONES)))
CONF_DUAL_ZONE_MODE = "dual_zone_mode"
CONF_FLOOR_WASHING_MODE = "floor_washing_mode"
CONF_ADD_WIRELESS_MODE = "add_wireless_mode"
CONF_CLOSE_ON_WIRELESS_LOST = "close_on_wireless_lost"
CONF_CHILD_LOCK = "child_lock"
CONF_WIRELESS_SENSOR_DISCHARGED = "wireless_sensor_discharged"
CONF_WIRELESS_SENSOR_LOST = "wireless_sensor_lost"
CONF_WIRE_LINE_LEAK = "wire_line_leak"
CONF_WIRE_LINE_LEAK_ARRAY = list(map(lambda x: CONF_WIRE_LINE_LEAK + "_" + str(1 + x), range(0, MAX_WIRE_LINE_LEAK_SENSORS)))
CONF_WIRELESS_SENSORS_COUNT = "wireless_sensors_count"
CONF_WATER_COUNTER = "water_counter"
CONF_WATER_COUNTER_ARRAY = list(map(lambda x: CONF_WATER_COUNTER + "_" + str(1 + x), range(0, MAX_WATER_COUNTERS)))


neptun_smart_ns = cg.esphome_ns.namespace("neptun_smart")
NeptunSmartComponent = neptun_smart_ns.class_("NeptunSmartComponent", cg.Component)
ValveZoneSwitch = neptun_smart_ns.class_("ValveZoneSwitch", switch.Switch)
DualZoneModeSwitch = neptun_smart_ns.class_("DualZoneModeSwitch", switch.Switch)
FloorModeSwitch = neptun_smart_ns.class_("FloorModeSwitch", switch.Switch)
AddWirelessModeSwitch = neptun_smart_ns.class_("AddWirelessModeSwitch", switch.Switch)
CloseOnWirelessLostSwitch = neptun_smart_ns.class_("CloseOnWirelessLostSwitch", switch.Switch)
ChildLockSwitch = neptun_smart_ns.class_("ChildLockSwitch", switch.Switch)

SCHEMA_ATTRS = {
    cv.GenerateID(): cv.declare_id(NeptunSmartComponent),
    cv.GenerateID(CONF_MODBUS_ID): cv.use_id(modbus_tcp.ModbusTcpComponent),
    cv.Optional(CONF_DEVICE_ID): cv.sub_device_id,
    cv.Optional(CONF_CONNECTION_STATUS): cv.maybe_simple_value(
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ), key=CONF_NAME,
    ),
    cv.Optional(CONF_DUAL_ZONE_MODE): cv.maybe_simple_value(
        switch.switch_schema(
            DualZoneModeSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="DISABLED",
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:lan-connect",
        ), key=CONF_NAME
    ),
    cv.Optional(CONF_FLOOR_WASHING_MODE): cv.maybe_simple_value(
        switch.switch_schema(
            FloorModeSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="DISABLED",
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:pail",
        ), key=CONF_NAME
    ),
    cv.Optional(CONF_ADD_WIRELESS_MODE): cv.maybe_simple_value(
        switch.switch_schema(
            AddWirelessModeSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="DISABLED",
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:plus-circle-multiple",
        ), key=CONF_NAME
    ),
    cv.Optional(CONF_CLOSE_ON_WIRELESS_LOST): cv.maybe_simple_value(
        switch.switch_schema(
            CloseOnWirelessLostSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="DISABLED",
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:pipe-disconnected",
        ), key=CONF_NAME
    ),
    cv.Optional(CONF_CHILD_LOCK): cv.maybe_simple_value(
        switch.switch_schema(
            ChildLockSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="DISABLED",
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:lock",
        ), key=CONF_NAME
    ),
    cv.Optional(CONF_WIRELESS_SENSOR_DISCHARGED): cv.maybe_simple_value(
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ), key=CONF_NAME,
    ),
    cv.Optional(CONF_WIRELESS_SENSOR_LOST): cv.maybe_simple_value(
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ), key=CONF_NAME,
    ),
    cv.Optional(CONF_WIRELESS_SENSORS_COUNT): cv.maybe_simple_value(
        sensor.sensor_schema(
            icon="mdi:access-point",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ), key=CONF_NAME,
    ),
}

for conf_id in CONF_ALERT_ZONE_ARRAY:
    SCHEMA_ATTRS[cv.Optional(conf_id)] = cv.maybe_simple_value(
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ), key=CONF_NAME,
    )

for conf_id in CONF_VALVE_ZONE_ARRAY:
    SCHEMA_ATTRS[cv.Optional(conf_id)] = cv.maybe_simple_value(
        switch.switch_schema(
            ValveZoneSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode="DISABLED",
            icon="mdi:water-pump",
        ), key=CONF_NAME
    )

for conf_id in CONF_WIRE_LINE_LEAK_ARRAY:
    SCHEMA_ATTRS[cv.Optional(conf_id)] = cv.maybe_simple_value(
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOISTURE,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ), key=CONF_NAME,
    )

for conf_id in CONF_WATER_COUNTER_ARRAY:
    SCHEMA_ATTRS[cv.Optional(conf_id)] = cv.maybe_simple_value(
        sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ), key=CONF_NAME,
    )

CONFIG_SCHEMA = cv.Schema(SCHEMA_ATTRS).extend(cv.COMPONENT_SCHEMA).extend(cv.polling_component_schema("10s"))

async def to_code(config):
    sub_device = None
    if sub_device_config := config.get(CONF_DEVICE_ID):
        sub_device = await cg.get_variable(sub_device_config)
    modbus_tcp_component = await cg.get_variable(config[CONF_MODBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], modbus_tcp_component)
    await cg.register_component(var, config)

    for key in [CONF_DUAL_ZONE_MODE, CONF_FLOOR_WASHING_MODE, CONF_ADD_WIRELESS_MODE, CONF_CLOSE_ON_WIRELESS_LOST, CONF_CHILD_LOCK]:
        if switch_config := config.get(key):
            swt = await switch.new_switch(switch_config)
            await cg.register_parented(swt, var)
            cg.add(getattr(swt, "set_has_state")(False))
            cg.add(getattr(var, f"set_{key}_switch")(swt))
            if sub_device:
                cg.add(getattr(swt, "set_device")(sub_device))

    for key in [CONF_CONNECTION_STATUS, CONF_WIRELESS_SENSOR_DISCHARGED, CONF_WIRELESS_SENSOR_LOST]:
        if sensor_config := config.get(key):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(sens, "set_trigger_on_initial_state")(True))
            cg.add(getattr(var, f"set_{key}_sensor")(sens))
            if sub_device:
                cg.add(getattr(sens, "set_device")(sub_device))

    for key in [CONF_WIRELESS_SENSORS_COUNT]:
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))
            if sub_device:
                cg.add(getattr(sens, "set_device")(sub_device))

    for idx, key in enumerate(CONF_ALERT_ZONE_ARRAY):
        if sensor_config := config.get(key):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(sens, "set_trigger_on_initial_state")(True))
            cg.add(getattr(var, f"set_{CONF_ALERT_ZONE}_sensor")(idx, sens))
            if sub_device:
                cg.add(getattr(sens, "set_device")(sub_device))

    for idx, key in enumerate(CONF_VALVE_ZONE_ARRAY):
        if switch_config := config.get(key):
            swt = await switch.new_switch(switch_config)
            await cg.register_parented(swt, var)
            cg.add(getattr(swt, "set_index")(idx))
            cg.add(getattr(swt, "set_has_state")(False))
            cg.add(getattr(var, f"set_{CONF_VALVE_ZONE}_switch")(idx, swt))
            if sub_device:
                cg.add(getattr(swt, "set_device")(sub_device))

    for idx, key in enumerate(CONF_WIRE_LINE_LEAK_ARRAY):
        if sensor_config := config.get(key):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(sens, "set_trigger_on_initial_state")(True))
            cg.add(getattr(var, f"set_{CONF_WIRE_LINE_LEAK}_sensor")(idx, sens))
            if sub_device:
                cg.add(getattr(sens, "set_device")(sub_device))

    for idx, key in enumerate(CONF_WATER_COUNTER_ARRAY):
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{CONF_WATER_COUNTER}_sensor")(idx, sens))
            if sub_device:
                cg.add(getattr(sens, "set_device")(sub_device))
