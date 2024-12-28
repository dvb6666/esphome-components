import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_SENSING_PIN,
    CONF_DIR_PIN,
    CONF_PERIOD,
    CONF_DELAY,
    CONF_TRIGGER_ID,
    CONF_FINGER_ID,
    CONF_FINGERPRINT_COUNT,
    CONF_LAST_FINGER_ID,
    CONF_ON_FINGER_SCAN_START,
    CONF_ON_FINGER_SCAN_MATCHED,
    CONF_ON_FINGER_SCAN_UNMATCHED,
    # CONF_ON_FINGER_SCAN_MISPLACED,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCOUNT,
    ICON_FINGERPRINT,
)

# from esphome.cpp_helpers import gpio_pin_expression

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor"]
MULTI_CONF = True

CONF_SENSOR_POWER_PIN = "sensor_power_pin"
CONF_IDLE_PERIOD_TO_SLEEP = "idle_period_to_sleep"
CONF_ERROR = "error"
CONF_ON_FINGER_SCAN_FAILED = "on_finger_scan_failed"
CONF_ROLE = "role"
CONF_COLOR_START = "start"
CONF_COLOR_END = "end"
CONF_ON_REGISTER_START = "on_register_step_start"
CONF_ON_REGISTER_DONE = "on_register_step_done"
CONF_ON_REGISTER_FAILED = "on_register_failed"

fingerprint_sfm_ns = cg.esphome_ns.namespace("fingerprint_sfm")
SfmComponent = fingerprint_sfm_ns.class_("SfmComponent", cg.Component, uart.UARTDevice)
SfmStartScanAction = fingerprint_sfm_ns.class_("SfmStartScanAction", automation.Action)
SfmStartRegisterAction = fingerprint_sfm_ns.class_("SfmStartRegisterAction", automation.Action)
SfmSetColorAction = fingerprint_sfm_ns.class_("SfmSetColorAction", automation.Action)
SfmDeleteAction = fingerprint_sfm_ns.class_("SfmDeleteAction", automation.Action)
SfmDeleteAllAction = fingerprint_sfm_ns.class_("SfmDeleteAllAction", automation.Action)
# SfmDelayAction = fingerprint_sfm_ns.class_("SfmDelayAction", automation.Action)

SfmScanStartTrigger = fingerprint_sfm_ns.class_("SfmScanStartTrigger", automation.Trigger.template())
SfmScanMatchedTrigger = fingerprint_sfm_ns.class_("SfmScanMatchedTrigger", automation.Trigger.template(cg.uint16, cg.uint8))
SfmScanUnmatchedTrigger = fingerprint_sfm_ns.class_("SfmScanUnmatchedTrigger", automation.Trigger.template())
SfmScanMisplacedTrigger = fingerprint_sfm_ns.class_("SfmScanMisplacedTrigger", automation.Trigger.template())
SfmScanFailedTrigger = fingerprint_sfm_ns.class_("SfmScanFailedTrigger", automation.Trigger.template(cg.uint8))
SfmRegisterStartTrigger = fingerprint_sfm_ns.class_("SfmRegisterStartTrigger", automation.Trigger.template(cg.uint8))
SfmRegisterDoneTrigger = fingerprint_sfm_ns.class_("SfmRegisterDoneTrigger", automation.Trigger.template(cg.uint8, cg.uint16))
SfmRegisterFailedTrigger = fingerprint_sfm_ns.class_("SfmRegisterFailedTrigger", automation.Trigger.template(cg.uint8))

SfmColor = fingerprint_sfm_ns.enum("SfmColor")
SFM_COLORS = {
    "YELLOW": SfmColor.YELLOW,
    "PURPLE": SfmColor.PURPLE,
    "RED": SfmColor.RED,
    "CYAN": SfmColor.CYAN,
    "GREEN": SfmColor.GREEN,
    "BLUE": SfmColor.BLUE,
    "OFF": SfmColor.OFF,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SfmComponent),
            cv.Required(CONF_SENSOR_POWER_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_SENSING_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_IDLE_PERIOD_TO_SLEEP): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DIR_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_FINGERPRINT_COUNT): sensor.sensor_schema(
                icon=ICON_FINGERPRINT,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LAST_FINGER_ID): sensor.sensor_schema(
                icon=ICON_ACCOUNT,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ON_FINGER_SCAN_START): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmScanStartTrigger)
            }),
            cv.Optional(CONF_ON_FINGER_SCAN_MATCHED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmScanMatchedTrigger)
            }),
            cv.Optional(CONF_ON_FINGER_SCAN_UNMATCHED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmScanUnmatchedTrigger)
            }),
            # cv.Optional(CONF_ON_FINGER_SCAN_MISPLACED): automation.validate_automation({
            #     cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmScanMisplacedTrigger)
            # }),
            cv.Optional(CONF_ON_FINGER_SCAN_FAILED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmScanFailedTrigger)
            }),
            cv.Optional(CONF_ON_REGISTER_START): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmRegisterStartTrigger)
            }),
            cv.Optional(CONF_ON_REGISTER_DONE): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmRegisterDoneTrigger)
            }),
            cv.Optional(CONF_ON_REGISTER_FAILED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SfmRegisterFailedTrigger)
            }),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

DEFAULT_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.use_id(SfmComponent),
})

START_REGISTER_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.use_id(SfmComponent),
    cv.Optional(CONF_FINGER_ID, default="0"): cv.templatable(cv.uint16_t),
    cv.Optional(CONF_ROLE, default="3"): cv.templatable(cv.uint8_t),
    cv.Optional(CONF_DELAY, default="2000ms"): cv.templatable(cv.positive_time_period_milliseconds),
})

SET_COLOR_ACTION_SCHEMA = cv.maybe_simple_value({
    cv.GenerateID(CONF_ID): cv.use_id(SfmComponent),
    cv.Required(CONF_COLOR_START): cv.templatable(cv.enum(SFM_COLORS, upper=True)),
    cv.Optional(CONF_COLOR_END, default="OFF"): cv.templatable(cv.enum(SFM_COLORS, upper=True)),
    cv.Optional(CONF_PERIOD, default="500ms"): cv.templatable(cv.positive_time_period_milliseconds),
    cv.Optional(CONF_DELAY, default="0ms"): cv.templatable(cv.positive_time_period_milliseconds),
}, key=CONF_COLOR_START)

DELETE_ACTION_SCHEMA = cv.maybe_simple_value({
    cv.GenerateID(CONF_ID): cv.use_id(SfmComponent),
    cv.Required(CONF_FINGER_ID): cv.templatable(cv.uint16_t),
}, key=CONF_FINGER_ID)

# DELAY_ACTION_SCHEMA = cv.maybe_simple_value({
#     cv.GenerateID(CONF_ID): cv.use_id(SfmComponent),
#     cv.Required(CONF_DELAY): cv.templatable(cv.positive_time_period_milliseconds),
# }, key=CONF_DELAY)

async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    sensor_power_pin = await cg.gpio_pin_expression(config.get(CONF_SENSOR_POWER_PIN))
    var = cg.new_Pvariable(config[CONF_ID], uart_component, sensor_power_pin)
    await cg.register_component(var, config)

    if sensing_pin_config := config.get(CONF_SENSING_PIN):
        sensing_pin = await cg.gpio_pin_expression(sensing_pin_config)
        cg.add(var.set_sensing_pin(sensing_pin))
    if CONF_IDLE_PERIOD_TO_SLEEP in config:
        idle_period_to_sleep_ms = config[CONF_IDLE_PERIOD_TO_SLEEP]
        cg.add(var.set_idle_period_to_sleep_ms(idle_period_to_sleep_ms))
    if dir_pin_config := config.get(CONF_DIR_PIN):
        dir_pin = await cg.gpio_pin_expression(dir_pin_config)
        cg.add(var.set_dir_pin(dir_pin))
    if error_config := config.get(CONF_ERROR):
        sens = await binary_sensor.new_binary_sensor(error_config)
        cg.add(var.set_error_sensor(sens))
    for key in [CONF_FINGERPRINT_COUNT, CONF_LAST_FINGER_ID]:
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    for conf in config.get(CONF_ON_FINGER_SCAN_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_FINGER_SCAN_MATCHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint16, "finger_id"), (cg.uint8, "role")], conf)
    for conf in config.get(CONF_ON_FINGER_SCAN_UNMATCHED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    # for conf in config.get(CONF_ON_FINGER_SCAN_MISPLACED, []):
    #     trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    #     await automation.build_automation(trigger, [], conf)
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


@automation.register_action("fingerprint_sfm.start_scan", SfmStartScanAction, DEFAULT_ACTION_SCHEMA)
async def start_scan_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action("fingerprint_sfm.start_register", SfmStartRegisterAction, START_REGISTER_ACTION_SCHEMA)
async def start_register_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_finger_id(await cg.templatable(config[CONF_FINGER_ID], args, cg.uint16)))
    cg.add(var.set_role(await cg.templatable(config[CONF_ROLE], args, cg.uint8)))
    cg.add(var.set_delay(await cg.templatable(config[CONF_DELAY], args, cg.uint32)))
    return var

@automation.register_action("fingerprint_sfm.set_color", SfmSetColorAction, SET_COLOR_ACTION_SCHEMA)
async def set_color_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_start(await cg.templatable(config[CONF_COLOR_START], args, SfmColor)))
    cg.add(var.set_end(await cg.templatable(config[CONF_COLOR_END], args, SfmColor)))
    cg.add(var.set_period(await cg.templatable(config[CONF_PERIOD], args, cg.uint16)))
    cg.add(var.set_delay(await cg.templatable(config[CONF_DELAY], args, cg.uint32)))
    return var

@automation.register_action("fingerprint_sfm.delete", SfmDeleteAction, DELETE_ACTION_SCHEMA)
async def delete_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_finger_id(await cg.templatable(config[CONF_FINGER_ID], args, cg.uint16)))
    return var

@automation.register_action("fingerprint_sfm.delete_all", SfmDeleteAllAction, DEFAULT_ACTION_SCHEMA)
async def delete_all_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

# @automation.register_action("fingerprint_sfm.delay", SfmDelayAction, DELAY_ACTION_SCHEMA)
# async def delay_action_to_code(config, action_id, template_arg, args):
#     var = cg.new_Pvariable(action_id, template_arg)
#     await cg.register_parented(var, config[CONF_ID])
#     cg.add(var.set_delay(await cg.templatable(config[CONF_DELAY], args, cg.uint32)))
#     return var
