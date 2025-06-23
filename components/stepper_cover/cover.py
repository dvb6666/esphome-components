import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.components import stepper
from esphome.const import (
    CONF_ID,
    CONF_TILT_ACTION,
    CONF_TILT_LAMBDA,
)

DEPENDENCIES = ["stepper"]

CONF_STEPPER = "stepper"
CONF_MAX_POSITION = "max_position"
CONF_RESTORE_MAX_POSITION = "restore_max_position"
CONF_RESTORE_TILT = "restore_tilt"
CONF_UPDATE_DELAY = "update_delay"

stepper_cover_ns = cg.esphome_ns.namespace("stepper_cover_")
StepperCover = stepper_cover_ns.class_("StepperCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.cover_schema(StepperCover).extend(
    {
        cv.GenerateID(): cv.declare_id(StepperCover),
        cv.Required(CONF_STEPPER): cv.use_id(stepper.Stepper),
        cv.Optional(CONF_MAX_POSITION, default=1000): cv.uint32_t,
        cv.Optional(CONF_RESTORE_MAX_POSITION, default=False): cv.boolean,
        cv.Optional(CONF_RESTORE_TILT, default=True): cv.boolean,
        cv.Optional(CONF_UPDATE_DELAY, default="500ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TILT_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_TILT_LAMBDA): cv.returning_lambda,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    stepper = await cg.get_variable(config[CONF_STEPPER])
    has_tilt_action = CONF_TILT_ACTION in config
    has_tilt_lambda = CONF_TILT_LAMBDA in config
    var = cg.new_Pvariable(config[CONF_ID], stepper, config[CONF_RESTORE_MAX_POSITION], has_tilt_action, has_tilt_lambda, config[CONF_RESTORE_TILT])
    cg.add(var.set_max_position(config[CONF_MAX_POSITION], False))
    cg.add(var.set_update_delay(config[CONF_UPDATE_DELAY]))
    if has_tilt_action:
        await automation.build_automation(
            var.get_tilt_trigger(), [(float, "tilt")], config[CONF_TILT_ACTION]
        )
    if has_tilt_lambda:
        tilt_template_ = await cg.process_lambda(
            config[CONF_TILT_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_tilt_lambda(tilt_template_))
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
