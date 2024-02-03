import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor
from esphome.components import sensor
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_ADDRESS,
    CONF_DIR_PIN,
    CONF_BATTERY_VOLTAGE,
    CONF_CURRENT,
    CONF_POWER,
    CONF_STARTUP_DELAY,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from esphome.cpp_helpers import gpio_pin_expression

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor"]
MULTI_CONF = True

MAX_TARIFF_COUNT = 4

CONF_ALL_COMMANDS = "all_commands"
CONF_ENERGY = list(map(lambda x: "energy" + str(1 + x), range(0, MAX_TARIFF_COUNT)))
CONF_ERROR = "error"

mercury200_ns = cg.esphome_ns.namespace("mercury200")
Mercury200 = mercury200_ns.class_("Mercury200", cg.PollingComponent, uart.UARTDevice)

SCHEMA_ATTRS = {
    cv.GenerateID(): cv.declare_id(Mercury200),
    cv.Required(CONF_ADDRESS): cv.uint32_t,
    cv.Optional(CONF_ALL_COMMANDS, default=False): cv.boolean,
    cv.Optional(CONF_DIR_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_STARTUP_DELAY, default="10s"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_POWER): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),

    cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

for conf_id in CONF_ENERGY:
    SCHEMA_ATTRS[cv.Optional(conf_id)] = sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


CONFIG_SCHEMA = cv.Schema(SCHEMA_ATTRS).extend(cv.polling_component_schema("60s")).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component, config[CONF_ADDRESS])
    cg.add(var.set_all_commands(config[CONF_ALL_COMMANDS]))
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))
    await cg.register_component(var, config)

    if dir_pin_config := config.get(CONF_DIR_PIN):
        dir_pin = await gpio_pin_expression(dir_pin_config)
        cg.add(var.set_dir_pin(dir_pin))

    if voltage_config := config.get(CONF_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))
    if current_config := config.get(CONF_CURRENT):
        sens = await sensor.new_sensor(current_config)
        cg.add(var.set_current_sensor(sens))
    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(var.set_power_sensor(sens))

    if battery_voltage_config := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(battery_voltage_config)
        cg.add(var.set_battery_voltage_sensor(sens))
    if error_config := config.get(CONF_ERROR):
        sens = await binary_sensor.new_binary_sensor(error_config)
        cg.add(var.set_error_binary_sensor(sens))

    for idx, conf_id in enumerate(CONF_ENERGY):
        if sens_config := config.get(conf_id):
            sens = await sensor.new_sensor(sens_config)
            cg.add(getattr(var, "set_energy_sensor")(idx, sens))
