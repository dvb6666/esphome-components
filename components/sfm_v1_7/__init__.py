import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_DIR_PIN,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from esphome.cpp_helpers import gpio_pin_expression

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor"]
MULTI_CONF = True

#CONF_TOUCH_MODE = "touch_mode"
CONF_VCC_PIN = "vcc_pin"
CONF_VCC_ALWAYS_ON="vcc_always_on"
CONF_IRQ_PIN = "irq_pin"
CONF_ERROR = "error"

sfm_v1_7_ns = cg.esphome_ns.namespace("sfm_v1_7")
SFM_v1_7 = sfm_v1_7_ns.class_("SFM_v1_7", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SFM_v1_7),
            cv.Required(CONF_VCC_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_VCC_ALWAYS_ON, default=False): cv.boolean,
            cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_DIR_PIN): pins.internal_gpio_output_pin_schema,
#            cv.Optional(CONF_TOUCH_MODE): cv.Schema(
#                {
#                    cv.Required(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
#                }
#            ),
            cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

#FORCE_UPDATE_ACTION_SCHEMA = cv.Schema({ cv.GenerateID(CONF_ID): cv.use_id(Nartis100) })


async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    vcc_pin = await cg.gpio_pin_expression(config.get(CONF_VCC_PIN))
    var = cg.new_Pvariable(config[CONF_ID], uart_component, vcc_pin, config[CONF_VCC_ALWAYS_ON])
    await cg.register_component(var, config)

    if irq_pin_config := config.get(CONF_IRQ_PIN):
        irq_pin = await cg.gpio_pin_expression(irq_pin_config)
        cg.add(var.set_irq_pin(irq_pin))
    if dir_pin_config := config.get(CONF_DIR_PIN):
        dir_pin = await cg.gpio_pin_expression(dir_pin_config)
        cg.add(var.set_dir_pin(dir_pin))
#    if touch_mode_config := config.get(CONF_TOUCH_MODE):
#        irq_pin = await cg.gpio_pin_expression(touch_mode_config[CONF_IRQ_PIN])
#        cg.add(var.set_touch_mode(irq_pin))
    if error_config := config.get(CONF_ERROR):
        sens = await binary_sensor.new_binary_sensor(error_config)
        cg.add(var.set_error_sensor(sens))
