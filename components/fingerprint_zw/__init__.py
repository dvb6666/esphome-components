import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_UART_ID,
    CONF_SENSING_PIN,
    CONF_COLOR,
    CONF_COUNT,
    CONF_STATE,
    CONF_DELAY,
    CONF_TRIGGER_ID,
    CONF_CAPACITY,
    CONF_FINGER_ID,
    CONF_FINGERPRINT_COUNT,
    CONF_LAST_FINGER_ID,
    CONF_ON_FINGER_SCAN_START,
    CONF_ON_FINGER_SCAN_MATCHED,
    CONF_ON_FINGER_SCAN_UNMATCHED,
    CONF_ON_FINGER_SCAN_MISPLACED,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCOUNT,
    ICON_FINGERPRINT,
    ICON_DATABASE,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor"]
MULTI_CONF = True

CONF_SENSOR_POWER_PIN = "sensor_power_pin"
CONF_IDLE_PERIOD_TO_SLEEP = "idle_period_to_sleep"
CONF_AUTO_LED_OFF = "auto_led_off"
CONF_ERROR = "error"
CONF_ON_FINGER_SCAN_FAILED = "on_finger_scan_failed"
CONF_ROLE = "role"
CONF_ON_REGISTER_START = "on_register_step_start"
CONF_ON_REGISTER_DONE = "on_register_step_done"
CONF_ON_REGISTER_FAILED = "on_register_failed"

fingerprint_zw_ns = cg.esphome_ns.namespace("fingerprint_zw")
ZwComponent = fingerprint_zw_ns.class_("ZwComponent", cg.Component, uart.UARTDevice)
ZwStartScanAction = fingerprint_zw_ns.class_("ZwStartScanAction", automation.Action)
ZwStartRegisterAction = fingerprint_zw_ns.class_("ZwStartRegisterAction", automation.Action)
ZwAuraLEDControlAction = fingerprint_zw_ns.class_("ZwAuraLEDControlAction", automation.Action)
ZwDeleteAction = fingerprint_zw_ns.class_("ZwDeleteAction", automation.Action)
ZwDeleteAllAction = fingerprint_zw_ns.class_("ZwDeleteAllAction", automation.Action)

ZwScanStartTrigger = fingerprint_zw_ns.class_("ZwScanStartTrigger", automation.Trigger.template())
ZwScanMatchedTrigger = fingerprint_zw_ns.class_("ZwScanMatchedTrigger", automation.Trigger.template(cg.uint16, cg.uint8))
ZwScanUnmatchedTrigger = fingerprint_zw_ns.class_("ZwScanUnmatchedTrigger", automation.Trigger.template())
ZwScanMisplacedTrigger = fingerprint_zw_ns.class_("ZwScanMisplacedTrigger", automation.Trigger.template())
ZwScanFailedTrigger = fingerprint_zw_ns.class_("ZwScanFailedTrigger", automation.Trigger.template(cg.uint8))
ZwRegisterStartTrigger = fingerprint_zw_ns.class_("ZwRegisterStartTrigger", automation.Trigger.template(cg.uint8))
ZwRegisterDoneTrigger = fingerprint_zw_ns.class_("ZwRegisterDoneTrigger", automation.Trigger.template(cg.uint8, cg.uint16))
ZwRegisterFailedTrigger = fingerprint_zw_ns.class_("ZwRegisterFailedTrigger", automation.Trigger.template(cg.uint8))

ZwAuraLEDState = fingerprint_zw_ns.enum("ZwAuraLEDState", True)
LED_STATES = {
    "BREATHING": ZwAuraLEDState.BREATHING,
    "FLASHING": ZwAuraLEDState.FLASHING,
    "ALWAYS_ON": ZwAuraLEDState.ALWAYS_ON,
    "ALWAYS_OFF": ZwAuraLEDState.ALWAYS_OFF,
    "GRADUAL_ON": ZwAuraLEDState.GRADUAL_ON,
    "GRADUAL_OFF": ZwAuraLEDState.GRADUAL_OFF,
}

ZwAuraLEDColor = fingerprint_zw_ns.enum("ZwAuraLEDColor")
LED_COLORS = {
    "OFF": ZwAuraLEDColor.OFF,
    "RED": ZwAuraLEDColor.RED,
    "BLUE": ZwAuraLEDColor.BLUE,
    "PURPLE": ZwAuraLEDColor.PURPLE,
    "GREEN": ZwAuraLEDColor.GREEN,
    "YELLOW": ZwAuraLEDColor.YELLOW,
    "CYAN": ZwAuraLEDColor.CYAN,
    "WHITE": ZwAuraLEDColor.WHITE,
}

def validate(config):
    if CONF_SENSOR_POWER_PIN in config and CONF_SENSING_PIN not in config:
        raise cv.Invalid("You cannot use the Sensor Power Pin without a Sensing Pin")
    if CONF_IDLE_PERIOD_TO_SLEEP in config and CONF_SENSOR_POWER_PIN not in config:
        raise cv.Invalid(
            "You cannot have an Idle Period to Sleep without a Sensor Power Pin"
        )
    return config

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZwComponent),
            cv.Optional(CONF_SENSOR_POWER_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_SENSING_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IDLE_PERIOD_TO_SLEEP): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_LED_OFF, default=False): cv.boolean,
            cv.Optional(CONF_ERROR): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_FINGERPRINT_COUNT): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_FINGERPRINT,
                    accuracy_decimals=0,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_LAST_FINGER_ID): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_ACCOUNT,
                    accuracy_decimals=0,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_CAPACITY): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_DATABASE,
                    accuracy_decimals=0,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ON_FINGER_SCAN_START): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwScanStartTrigger)
            }),
            cv.Optional(CONF_ON_FINGER_SCAN_MATCHED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwScanMatchedTrigger)
            }),
            cv.Optional(CONF_ON_FINGER_SCAN_UNMATCHED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwScanUnmatchedTrigger)
            }),
            cv.Optional(CONF_ON_FINGER_SCAN_MISPLACED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwScanMisplacedTrigger)
            }),
            cv.Optional(CONF_ON_FINGER_SCAN_FAILED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwScanFailedTrigger)
            }),
            cv.Optional(CONF_ON_REGISTER_START): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwRegisterStartTrigger)
            }),
            cv.Optional(CONF_ON_REGISTER_DONE): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwRegisterDoneTrigger)
            }),
            cv.Optional(CONF_ON_REGISTER_FAILED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZwRegisterFailedTrigger)
            }),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

DEFAULT_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.use_id(ZwComponent),
})

START_REGISTER_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.use_id(ZwComponent),
    cv.Optional(CONF_FINGER_ID, default="0"): cv.templatable(cv.uint16_t),
    cv.Optional(CONF_ROLE, default="3"): cv.templatable(cv.uint8_t),
    cv.Optional(CONF_DELAY, default="2000ms"): cv.templatable(cv.positive_time_period_milliseconds),
})

SET_COLOR_ACTION_SCHEMA = cv.maybe_simple_value({
    cv.GenerateID(CONF_ID): cv.use_id(ZwComponent),
    cv.Optional(CONF_STATE, default="BREATHING"): cv.templatable(cv.enum(LED_STATES, upper=True)),
    cv.Required(CONF_COLOR): cv.templatable(cv.enum(LED_COLORS, upper=True)),
    cv.Optional(CONF_COUNT, default="0"): cv.templatable(cv.uint8_t),
}, key=CONF_COLOR)

DELETE_ACTION_SCHEMA = cv.maybe_simple_value({
    cv.GenerateID(CONF_ID): cv.use_id(ZwComponent),
    cv.Required(CONF_FINGER_ID): cv.templatable(cv.uint16_t),
}, key=CONF_FINGER_ID)

# DELAY_ACTION_SCHEMA = cv.maybe_simple_value({
#     cv.GenerateID(CONF_ID): cv.use_id(ZwComponent),
#     cv.Required(CONF_DELAY): cv.templatable(cv.positive_time_period_milliseconds),
# }, key=CONF_DELAY)

async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)

    if sensor_power_pin_config := config.get(CONF_SENSOR_POWER_PIN):
        sensor_pin = await cg.gpio_pin_expression(sensor_power_pin_config)
        cg.add(var.set_sensor_power_pin(sensor_pin))
    if sensing_pin_config := config.get(CONF_SENSING_PIN):
        sensing_pin = await cg.gpio_pin_expression(sensing_pin_config)
        cg.add(var.set_sensing_pin(sensing_pin))
    for key in [CONF_IDLE_PERIOD_TO_SLEEP, CONF_AUTO_LED_OFF]:
        if var_config := config.get(key):
            cg.add(getattr(var, f"set_{key}")(var_config))
    if error_config := config.get(CONF_ERROR):
        sens = await binary_sensor.new_binary_sensor(error_config)
        cg.add(var.set_error_sensor(sens))
    for key in [CONF_FINGERPRINT_COUNT, CONF_LAST_FINGER_ID, CONF_CAPACITY]:
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    for conf in config.get(CONF_ON_FINGER_SCAN_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_FINGER_SCAN_MATCHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint16, "finger_id"), (cg.uint16, "score")], conf)
    for conf in config.get(CONF_ON_FINGER_SCAN_UNMATCHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_FINGER_SCAN_MISPLACED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_FINGER_SCAN_FAILED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "error_code")], conf)
    for conf in config.get(CONF_ON_REGISTER_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "step")], conf)
    for conf in config.get(CONF_ON_REGISTER_DONE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "step"), (cg.uint16, "finger_id")], conf)
    for conf in config.get(CONF_ON_REGISTER_FAILED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "error_code")], conf)


@automation.register_action("fingerprint_zw.start_scan", ZwStartScanAction, DEFAULT_ACTION_SCHEMA)
async def start_scan_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

# @automation.register_action("fingerprint_zw.start_register", ZwStartRegisterAction, START_REGISTER_ACTION_SCHEMA)
# async def start_register_action_to_code(config, action_id, template_arg, args):
#     var = cg.new_Pvariable(action_id, template_arg)
#     await cg.register_parented(var, config[CONF_ID])
#     cg.add(var.set_finger_id(await cg.templatable(config[CONF_FINGER_ID], args, cg.uint16)))
#     cg.add(var.set_role(await cg.templatable(config[CONF_ROLE], args, cg.uint8)))
#     cg.add(var.set_delay(await cg.templatable(config[CONF_DELAY], args, cg.uint32)))
#     return var

@automation.register_action("fingerprint_zw.aura_led_control", ZwAuraLEDControlAction, SET_COLOR_ACTION_SCHEMA)
async def set_color_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, ZwAuraLEDState)))
    cg.add(var.set_color(await cg.templatable(config[CONF_COLOR], args, ZwAuraLEDColor)))
    cg.add(var.set_count(await cg.templatable(config[CONF_COUNT], args, cg.uint8)))
    return var

@automation.register_action("fingerprint_zw.delete", ZwDeleteAction, DELETE_ACTION_SCHEMA)
async def delete_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_finger_id(await cg.templatable(config[CONF_FINGER_ID], args, cg.uint16)))
    return var

@automation.register_action("fingerprint_zw.delete_all", ZwDeleteAllAction, DEFAULT_ACTION_SCHEMA)
async def delete_all_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

# @automation.register_action("fingerprint_zw.delay", ZwDelayAction, DELAY_ACTION_SCHEMA)
# async def delay_action_to_code(config, action_id, template_arg, args):
#     var = cg.new_Pvariable(action_id, template_arg)
#     await cg.register_parented(var, config[CONF_ID])
#     cg.add(var.set_delay(await cg.templatable(config[CONF_DELAY], args, cg.uint32)))
#     return var
