import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import valve
from esphome.const import (
    CONF_ID,
    # CONF_INPUT,
    # CONF_OUTPUT,
    CONF_DEVICE_CLASS,
    DEVICE_CLASS_WATER,
)

DEPENDENCIES = ["valve"]

CONF_CLOSE_PIN = "close_pin"
CONF_OPEN_PIN = "open_pin"
CONF_ALARM_PIN = "alarm_pin"
CONF_IGNORE_BUTTONS = "ignore_buttons"

aqua_watchman_ns = cg.esphome_ns.namespace("aqua_watchman")
AquaWatchmanValve = aqua_watchman_ns.class_("AquaWatchmanValve", valve.Valve, cg.Component)
AquaWatchmanAlarmAction = aqua_watchman_ns.class_("AquaWatchmanAlarmAction", automation.Action)

CONFIG_SCHEMA = valve.VALVE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AquaWatchmanValve),
        cv.Required(CONF_CLOSE_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_OPEN_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_ALARM_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_IGNORE_BUTTONS, default=False): cv.boolean,
        cv.Optional(CONF_DEVICE_CLASS, default=DEVICE_CLASS_WATER): cv.one_of(*valve.DEVICE_CLASSES, lower=True, space="_"),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    close_pin = await cg.gpio_pin_expression(config[CONF_CLOSE_PIN])
    open_pin = await cg.gpio_pin_expression(config[CONF_OPEN_PIN])
    var = await valve.new_valve(config, close_pin, open_pin)
    await cg.register_component(var, config)

    # for key in [CONF_CLOSE_PINS, CONF_OPEN_PINS, CONF_ALARM_PINS]:
    #     if pin_config := config.get(key):
    #         output_pin = await cg.gpio_pin_expression(pin_config.get(CONF_OUTPUT))
    #         if input_config := pin_config.get(CONF_INPUT):
    #             input_pin = await cg.gpio_pin_expression(input_config)
    #             cg.add(input_pin.set_inverted(True))
    #             cg.add(getattr(var, f"set_{key}")(output_pin, input_pin))
    #         else:
    #             cg.add(getattr(var, f"set_{key}")(output_pin))
    if alarm_pin_config := config.get(CONF_ALARM_PIN):
        alarm_pin = await cg.gpio_pin_expression(alarm_pin_config)
        cg.add(var.set_alarm_pin(alarm_pin))
    cg.add(var.set_ignore_buttons(config[CONF_IGNORE_BUTTONS]))

@automation.register_action("valve.aqua_watchman.alarm", AquaWatchmanAlarmAction, valve.VALVE_ACTION_SCHEMA)
async def aqua_watchman_alarm_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
