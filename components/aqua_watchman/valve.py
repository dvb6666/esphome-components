import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, valve
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    # ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_WATER,
)

ESPHOME_MIN_VERSION = "2025.5.2"
DEPENDENCIES = ["valve"]
AUTO_LOAD = ["binary_sensor"]

CONF_CLOSE_PIN = "close_pin"
CONF_OPEN_PIN = "open_pin"
CONF_ALARM_PIN = "alarm_pin"
CONF_POWER_PIN = "power_pin"
CONF_IGNORE_BUTTONS = "ignore_buttons"
CONF_FLOOR_CLEANING_SENSOR = "floor_cleaning_sensor"
CONF_POWER_SENSOR = "power_sensor"

aqua_watchman_ns = cg.esphome_ns.namespace("aqua_watchman")
AquaWatchmanValve = aqua_watchman_ns.class_("AquaWatchmanValve", valve.Valve, cg.Component)
AquaWatchmanAlarmAction = aqua_watchman_ns.class_("AquaWatchmanAlarmAction", automation.Action)

def validate(config):
    cv.All(cv.version_number, cv.validate_esphome_version)(ESPHOME_MIN_VERSION)
    if CONF_FLOOR_CLEANING_SENSOR in config and CONF_ALARM_PIN not in config:
        raise cv.Invalid("You cannot use the Floor Cleaning Sensor without a Alarm Pin")
    if CONF_POWER_SENSOR in config and CONF_POWER_PIN not in config:
        raise cv.Invalid("You cannot use the Power Sensor without a Power Pin")
    return config

CONFIG_SCHEMA = cv.All(valve.valve_schema(device_class = DEVICE_CLASS_WATER).extend(
    {
        cv.GenerateID(): cv.declare_id(AquaWatchmanValve),
        cv.Required(CONF_CLOSE_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_OPEN_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_ALARM_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_POWER_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_FLOOR_CLEANING_SENSOR): cv.maybe_simple_value(
            binary_sensor.binary_sensor_schema(
                icon="mdi:pail",
                # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_POWER_SENSOR): cv.maybe_simple_value(
            binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_POWER,
                # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_IGNORE_BUTTONS, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA), validate)


async def to_code(config):
    close_pin = await cg.gpio_pin_expression(config[CONF_CLOSE_PIN])
    open_pin = await cg.gpio_pin_expression(config[CONF_OPEN_PIN])
    var = await valve.new_valve(config, close_pin, open_pin)
    await cg.register_component(var, config)
    for key in [CONF_ALARM_PIN, CONF_POWER_PIN]:
        if pin_config := config.get(key):
            pin = await cg.gpio_pin_expression(pin_config)
            cg.add(getattr(var, f"set_{key}")(pin))
    for key in [CONF_FLOOR_CLEANING_SENSOR, CONF_POWER_SENSOR]:
        if sensor_config := config.get(key):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(var, f"set_{key}")(sens))
    cg.add(var.set_ignore_buttons(config[CONF_IGNORE_BUTTONS]))


@automation.register_action("valve.aqua_watchman.alarm", AquaWatchmanAlarmAction, valve.VALVE_ACTION_SCHEMA)
async def aqua_watchman_alarm_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
