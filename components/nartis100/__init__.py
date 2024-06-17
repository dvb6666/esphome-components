import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_PASSWORD,
    CONF_DIR_PIN,
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
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
MULTI_CONF = True

MAX_TARIFF_COUNT = 4

CONF_ENERGY = list(map(lambda x: "energy" + str(1 + x), range(0, MAX_TARIFF_COUNT)))
CONF_SERIAL_NUMBER = "serial_number"
CONF_RELEASE_DATE = "release_date"
CONF_ERROR = "error"

nartis100_ns = cg.esphome_ns.namespace("nartis100")
Nartis100 = nartis100_ns.class_("Nartis100", cg.PollingComponent, uart.UARTDevice)
# Actions
Nartis100ForceUpdateAction = nartis100_ns.class_("Nartis100ForceUpdateAction", automation.Action)

SCHEMA_ATTRS = {
    cv.GenerateID(): cv.declare_id(Nartis100),
#    cv.Optional(CONF_PASSWORD, default="111"): cv.string,
    cv.Required(CONF_PASSWORD): cv.All(cv.string, cv.Length(min=3,max=8)),
    cv.Optional(CONF_DIR_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_STARTUP_DELAY, default="10s"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_POWER): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_RELEASE_DATE): text_sensor.text_sensor_schema(),
}

for conf_id in CONF_ENERGY:
    SCHEMA_ATTRS[cv.Optional(conf_id)] = sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


CONFIG_SCHEMA = cv.Schema(SCHEMA_ATTRS).extend(cv.polling_component_schema("60s")).extend(uart.UART_DEVICE_SCHEMA)

FORCE_UPDATE_ACTION_SCHEMA = cv.Schema({ cv.GenerateID(CONF_ID): cv.use_id(Nartis100) })


async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component, config[CONF_PASSWORD])
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))
    await cg.register_component(var, config)

    if dir_pin_config := config.get(CONF_DIR_PIN):
        dir_pin = await gpio_pin_expression(dir_pin_config)
        cg.add(var.set_dir_pin(dir_pin))

    if current_config := config.get(CONF_CURRENT):
        sens = await sensor.new_sensor(current_config)
        cg.add(var.set_current_sensor(sens))
    if voltage_config := config.get(CONF_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))
    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(var.set_power_sensor(sens))

    if error_config := config.get(CONF_ERROR):
        sens = await binary_sensor.new_binary_sensor(error_config)
        cg.add(var.set_error_binary_sensor(sens))

    for idx, conf_id in enumerate(CONF_ENERGY):
        if sens_config := config.get(conf_id):
            sens = await sensor.new_sensor(sens_config)
            cg.add(getattr(var, "set_energy_sensor")(idx, sens))

    if serial_number_config := config.get(CONF_SERIAL_NUMBER):
        serial_number = await text_sensor.new_text_sensor(serial_number_config)
        cg.add(var.set_serial_number_sensor(serial_number))
    if release_date_config := config.get(CONF_RELEASE_DATE):
        release_date = await text_sensor.new_text_sensor(release_date_config)
        cg.add(var.set_release_date_sensor(release_date))

@automation.register_action("nartis100.force_update", Nartis100ForceUpdateAction, FORCE_UPDATE_ACTION_SCHEMA)
async def update_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var
