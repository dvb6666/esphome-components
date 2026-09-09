import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_DEVICE_ID,
    CONF_UART_ID,
    CONF_ADDRESS,
    CONF_DATETIME,
    CONF_HOURS,
    CONF_POWER,
    CONF_FLOW,
    CONF_VOLUME,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
#    DEVICE_CLASS_SAFETY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_TIMESTAMP,
    DEVICE_CLASS_VOLUME,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_HOUR,
)

DEPENDENCIES = ['uart']
AUTO_LOAD = ['binary_sensor', 'sensor', 'text_sensor']
MULTI_CONF = True

CONF_UNIT_FIRST = 'unit_first'

CONF_COOLING_ENERGY = 'cooling_energy'
CONF_HEATING_ENERGY = 'heating_energy'
CONF_WATER_SUPPLY_TEMPERATURE = 'water_supply_temperature'
CONF_BACKWATER_TEMPERATURE = 'backwater_temperature'

CONF_CONNECTIVITY_ERROR = 'connectivity_error'
CONF_BATTERY_POWER_ALARM = 'battery_power_alarm'
CONF_FLOW_ALARM = 'flow_alarm'
CONF_EE_FAULT = 'ee_fault'
CONF_TEMPERATURE_LESS_3_DEGREE = 'temperature_less_3_degree'
CONF_TEMPERATURE_MORE_95_DEGREE = 'temperature_more_95_degree'

sanext_ns = cg.esphome_ns.namespace('sanext_mono_cu')
SanextMonoCU = sanext_ns.class_('SanextMonoCU', cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SanextMonoCU),
            cv.Optional(CONF_DEVICE_ID): cv.sub_device_id,
            cv.Optional(CONF_UNIT_FIRST, default=False): cv.boolean,
            cv.Optional(CONF_ADDRESS): cv.hex_uint64_t,

            cv.Optional(CONF_COOLING_ENERGY): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_ENERGY,
                    icon="mdi:snowflake-thermometer",
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                    unit_of_measurement=UNIT_KILOWATT_HOURS,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_HEATING_ENERGY): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_ENERGY,
                    icon="mdi:sun-thermometer",
                    state_class=STATE_CLASS_TOTAL_INCREASING,
                    unit_of_measurement=UNIT_KILOWATT_HOURS,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_POWER): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_POWER,
                    state_class=STATE_CLASS_MEASUREMENT,
                    unit_of_measurement=UNIT_KILOWATT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_FLOW): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
                    state_class=STATE_CLASS_MEASUREMENT,
                    unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_VOLUME): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_VOLUME,
                    state_class=STATE_CLASS_MEASUREMENT,
                    unit_of_measurement=UNIT_CUBIC_METER,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_WATER_SUPPLY_TEMPERATURE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    unit_of_measurement=UNIT_CELSIUS,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_BACKWATER_TEMPERATURE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    unit_of_measurement=UNIT_CELSIUS,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_HOURS): cv.maybe_simple_value(
                sensor.sensor_schema(
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                    unit_of_measurement=UNIT_HOUR,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_DATETIME): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(
#                    device_class=DEVICE_CLASS_TIMESTAMP,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),

            cv.Optional(CONF_CONNECTIVITY_ERROR): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_BATTERY_POWER_ALARM): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_BATTERY,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_FLOW_ALARM): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                    icon="mdi:waves-arrow-right",
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_EE_FAULT): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_PROBLEM,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                    icon="mdi:sd",
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_TEMPERATURE_LESS_3_DEGREE): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_COLD,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_TEMPERATURE_MORE_95_DEGREE): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_HEAT,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60min"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    sub_device = None
    if (sub_device_config := config.get(CONF_DEVICE_ID)):
        sub_device = await cg.get_variable(sub_device_config)
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component, config[CONF_UNIT_FIRST])
    await cg.register_component(var, config)
    if (address := config.get(CONF_ADDRESS)) is not None:
        cg.add(var.set_address(address))
    for key in [CONF_COOLING_ENERGY, CONF_HEATING_ENERGY, CONF_POWER, CONF_FLOW, CONF_VOLUME, CONF_WATER_SUPPLY_TEMPERATURE, CONF_BACKWATER_TEMPERATURE, CONF_HOURS]:
        if (sensor_config := config.get(key)):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f'set_{key}_sensor')(sens))
            if sub_device:
                cg.add(getattr(sens, 'set_device_')(sub_device))
    for key in [CONF_CONNECTIVITY_ERROR, CONF_BATTERY_POWER_ALARM, CONF_FLOW_ALARM, CONF_EE_FAULT, CONF_TEMPERATURE_LESS_3_DEGREE, CONF_TEMPERATURE_MORE_95_DEGREE]:
        if (sensor_config := config.get(key)):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(var, f'set_{key}_sensor')(sens))
            if sub_device:
                cg.add(getattr(sens, 'set_device_')(sub_device))
    for key in [CONF_DATETIME]:
        if (sensor_config := config.get(key)):
            sens = await text_sensor.new_text_sensor(sensor_config)
            cg.add(getattr(var, f'set_{key}_sensor')(sens))
            if sub_device:
                cg.add(getattr(sens, 'set_device_')(sub_device))
