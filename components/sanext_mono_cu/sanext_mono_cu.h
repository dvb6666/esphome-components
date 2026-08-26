#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include <memory>
#include <queue>

namespace esphome {
namespace sanext_mono_cu {

#define TX_BUFFER_SIZE 64
#define RX_BUFFER_SIZE 192
#define DEFAULT_ADDRESS 0xAAAAAAAAAAAAAAUL
class SanextCommand {
 public:
  SanextCommand(uint8_t code_, uint8_t request_length_, uint8_t response_length_, uint8_t d0_ = 0, uint8_t d1_ = 0)
      : code(code_), request_length(request_length_), response_length(response_length_), d0(d0_), d1(d1_) {}
  uint8_t code, request_length, response_length, d0, d1;
};

#define SANEXT_ReadMeter 0x01

class SanextCommandReadMeter : public SanextCommand {
 public:
  SanextCommandReadMeter() : SanextCommand(SANEXT_ReadMeter, 3, 0x2E, 0x1F, 0x90) {}
};


class SanextMonoCU : public PollingComponent, public uart::UARTDevice {
 public:
  SanextMonoCU(uart::UARTComponent *uart, bool unit_first) : uart::UARTDevice(uart), unit_first_(unit_first) {}

  static uint8_t bcd8(const uint8_t data) { return (data & 0x0f) + 10 * ((data >> 4) & 0x0f); };
  static uint16_t bcd16(const uint8_t *data) { return bcd8(data[0]) + 100 * bcd8(data[1]); };
  static uint32_t bcd24(const uint8_t *data) { return bcd16(data) + 10000 * bcd8(data[2]); };
  static uint32_t bcd32(const uint8_t *data) { return bcd16(data) + 10000 * bcd16(data + 2); };
  /*static uint16_t htons(uint16_t a) { return ((a >> 8) & 0xff) | ((a & 0xff) << 8); };
  static uint32_t htonl(uint32_t a) {
    return ((a & 0xff000000) >> 24) | ((a & 0xff0000) >> 8) | ((a & 0xff00) << 8) | ((a & 0xff) << 24);
  };*/

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_cooling_energy_sensor(sensor::Sensor *sensor) { this->cooling_energy_sensor_ = sensor; }
  void set_heating_energy_sensor(sensor::Sensor *sensor) { this->heating_energy_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_flow_sensor(sensor::Sensor *sensor) { this->flow_sensor_ = sensor; }
  void set_volume_sensor(sensor::Sensor *sensor) { this->volume_sensor_ = sensor; }
  void set_water_supply_temperature_sensor(sensor::Sensor *sensor) { this->water_supply_temperature_sensor_ = sensor; }
  void set_backwater_temperature_sensor(sensor::Sensor *sensor) { this->backwater_temperature_sensor_ = sensor; }
  void set_hours_sensor(sensor::Sensor *sensor) { this->hours_sensor_ = sensor; }
  void set_connectivity_error_sensor(binary_sensor::BinarySensor *sensor) { this->connectivity_error_sensor_ = sensor; }
  void set_battery_power_alarm_sensor(binary_sensor::BinarySensor *sensor) { this->battery_power_alarm_sensor_ = sensor; }
  void set_flow_alarm_sensor(binary_sensor::BinarySensor *sensor) { this->flow_alarm_sensor_ = sensor; }
  void set_ee_fault_sensor(binary_sensor::BinarySensor *sensor) { this->ee_fault_sensor_ = sensor; }
  void set_temperature_less_3_degree_sensor(binary_sensor::BinarySensor *sensor) { this->temperature_less_3_degree_sensor_ = sensor; }
  void set_temperature_more_95_degree_sensor(binary_sensor::BinarySensor *sensor) { this->temperature_more_95_degree_sensor_ = sensor; }
  void set_datetime_sensor(text_sensor::TextSensor *sensor) { this->datetime_sensor_ = sensor; }

  void set_address(uint64_t address) { this->address_ = address; };
  void read_meter();

 protected:
  void delay(uint32_t delay_ms) { this->sleep_time_ = millis() + delay_ms; }
  bool process_command(SanextCommand *command);
  bool process_error(SanextCommand *command, uint8_t error_code = 0x01);

 private:
  sensor::Sensor *cooling_energy_sensor_{nullptr}, *heating_energy_sensor_{nullptr}, *power_sensor_{nullptr}, *flow_sensor_{nullptr},
    *volume_sensor_{nullptr}, *water_supply_temperature_sensor_{nullptr}, *backwater_temperature_sensor_{nullptr}, *hours_sensor_{nullptr};
  binary_sensor::BinarySensor *connectivity_error_sensor_{nullptr}, *battery_power_alarm_sensor_{nullptr}, *flow_alarm_sensor_{nullptr},
    *ee_fault_sensor_{nullptr}, *temperature_less_3_degree_sensor_{nullptr}, *temperature_more_95_degree_sensor_{nullptr};
  text_sensor::TextSensor *datetime_sensor_{nullptr};
  std::queue<std::unique_ptr<SanextCommand>> commands_queue_;
  bool running_{false}, error_{false}, unit_first_;
  uint64_t address_{DEFAULT_ADDRESS};
  uint16_t phase_{0}, process_phase_{0}, retry_count_{0};
  unsigned long sleep_time_{0}, wait_time_{0}, log_time_{0};
  uint16_t tx_bytes_sending_{0}, rx_bytes_needed_{0}, rx_bytes_received_{0};
  uint8_t tx_buffer_[TX_BUFFER_SIZE], rx_buffer_[RX_BUFFER_SIZE];
  uint8_t serial_{0};
};

}  // namespace sanext_mono_cu
}  // namespace esphome
