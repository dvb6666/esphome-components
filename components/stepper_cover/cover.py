import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.components import stepper
from esphome.const import (
    CONF_ID,
)

DEPENDENCIES = ["stepper"]

CONF_STEPPER = "stepper"
CONF_MAX_POSITION = "max_position"
CONF_RESTORE_MAX_POSITION = "restore_max_position"
CONF_UPDATE_DELAY = "update_delay"

stepper_cover_ns = cg.esphome_ns.namespace("stepper_cover_")
StepperCover = stepper_cover_ns.class_("StepperCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(StepperCover),
        cv.Required(CONF_STEPPER): cv.use_id(stepper.Stepper),
        cv.Optional(CONF_MAX_POSITION, default=1000): cv.uint32_t,
        cv.Optional(CONF_RESTORE_MAX_POSITION, default=False): cv.boolean,
        cv.Optional(CONF_UPDATE_DELAY, default="500ms"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    stepper = await cg.get_variable(config[CONF_STEPPER])
    var = cg.new_Pvariable(config[CONF_ID], stepper, config[CONF_RESTORE_MAX_POSITION])
    cg.add(var.set_max_position(config[CONF_MAX_POSITION], False))
    cg.add(var.set_update_delay(config[CONF_UPDATE_DELAY]))
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
