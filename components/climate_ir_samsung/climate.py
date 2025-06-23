import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import climate, sensor
from esphome.const import (
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_SENSOR,
)

AUTO_LOAD = ["sensor"]

CONF_IR_LED = "ir_led"
CONF_DEFAULT_TEMPERATURE = "default_temperature"

climate_ir_samsung_ns = cg.esphome_ns.namespace("climate_ir_samsung")
SamsungAC = climate_ir_samsung_ns.class_("SamsungAC", climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(SamsungAC).extend(
        {
            cv.GenerateID(): cv.declare_id(SamsungAC),
            cv.Required(CONF_IR_LED): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_DEFAULT_TEMPERATURE, default="25°C"): cv.temperature,
            cv.Optional(CONF_MIN_TEMPERATURE, default="16°C"): cv.temperature,
            cv.Optional(CONF_MAX_TEMPERATURE, default="30°C"): cv.temperature,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)

async def to_code(config):
    ir_pin = await cg.gpio_pin_expression(config[CONF_IR_LED])
    var = cg.new_Pvariable(config[CONF_ID], ir_pin, config[CONF_DEFAULT_TEMPERATURE],
                           config[CONF_MIN_TEMPERATURE], config[CONF_MAX_TEMPERATURE])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    if sensor_config := config.get(CONF_SENSOR):
        sens = await cg.get_variable(sensor_config)
        cg.add(var.set_sensor(sens))
    cg.add_library("IRremoteESP8266", None)
