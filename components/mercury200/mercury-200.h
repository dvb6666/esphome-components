#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "time.h"

namespace esphome {
namespace mercury200 {

#define MAX_TARIFF_COUNT 4
#define TX_BUFFER_SIZE 7 + 1
#define RX_BUFFER_SIZE 7 + 16

class Command {
public:
  Command(uint8_t code, uint8_t data_size) : code_(code), data_size_(data_size) {}
  uint8_t code() { return this->code_; }
  uint8_t data_size() { return this->data_size_; }
  virtual void process(const uint8_t *data) = 0;
  static uint8_t bcd(const uint8_t data) { return (data & 0x0f) + 10 * ((data >> 4) & 0x0f); };
  static uint16_t bcd16(const uint8_t *data, uint8_t len = 2);
  static uint32_t bcd32(const uint8_t *data, uint8_t len = 4);
  static uint16_t uint16(const uint8_t *data, uint8_t len = 2);
  static uint32_t uint32(const uint8_t *data, uint8_t len = 4);
  static uint16_t htons(uint16_t a) { return ((a >> 8) & 0xff) | ((a & 0xff) << 8); };
  static uint32_t htonl(uint32_t a) { return ((a & 0xff000000) >> 24) | ((a & 0xff0000) >> 8) | ((a & 0xff00) << 8) | ((a & 0xff) << 24); };
  static time_t timestamp(const uint8_t *data);

protected:
  uint8_t code_, data_size_;
};

class GetSerialNumberCommand : public Command {
public:
  GetSerialNumberCommand(std::function<void(uint32_t)> on_value) : Command(0x2F, 4), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(uint32(data)); }
  std::function<void(uint32_t addr)> on_value_;
};

class GetVersionCommand : public Command {
public:
  GetVersionCommand(std::function<void(uint16_t, uint32_t)> on_value) : Command(0x28, 6), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(uint16(data), uint32(data + 2)); }
  std::function<void(uint16_t ver, uint32_t data_ver)> on_value_;
};

class GetBatteryCommand : public Command {
public:
  GetBatteryCommand(std::function<void(float)> on_value) : Command(0x29, 2), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_((float)bcd16(data) / 100); }
  std::function<void(float voltage)> on_value_;
};

class GetTimeCommand : public Command {
public:
  GetTimeCommand(std::function<void(uint32_t)> on_value) : Command(0x21, 7), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(timestamp(data)); }
  std::function<void(uint32_t tm)> on_value_;
};

class GetLastTurnOffCommand : public Command {
public:
  GetLastTurnOffCommand(std::function<void(uint32_t)> on_value) : Command(0x2B, 7), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(timestamp(data)); }
  std::function<void(uint32_t tm)> on_value_;
};

class GetLastTurnOnCommand : public Command {
public:
  GetLastTurnOnCommand(std::function<void(uint32_t)> on_value) : Command(0x2C, 7), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(timestamp(data)); }
  std::function<void(uint32_t tm)> on_value_;
};

class GetTarifsCountCommand : public Command {
public:
  GetTarifsCountCommand(std::function<void(uint8_t)> on_value) : Command(0x2E, 1), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(data[0]); }
  std::function<void(uint8_t count)> on_value_;
};

class GetDateFabricCommand : public Command {
public:
  GetDateFabricCommand(std::function<void(uint8_t, uint8_t, uint16_t)> on_value) : Command(0x66, 3), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_(bcd(data[0]), bcd(data[1]), 2000 + bcd(data[2])); }
  std::function<void(uint8_t day, uint8_t month, uint16_t year)> on_value_;
};

class GetUIPCommand : public Command {
public:
  GetUIPCommand(std::function<void(float, float, uint32_t)> on_value) : Command(0x63, 7), on_value_(on_value) {}
  void process(const uint8_t *data) override { on_value_((float)bcd16(data) / 10, (float)bcd16(data + 2) / 100, bcd32(data + 4, 3)); }
  std::function<void(float v, float i, uint32_t p)> on_value_;
};

class GetCountersCommand : public Command {
public:
  GetCountersCommand(std::function<void(float, float, float, float)> on_value) : Command(0x27, 16), on_value_(on_value) {}
  void process(const uint8_t *data) override {
    on_value_((float)bcd32(data) / 100, (float)bcd32(data + 4) / 100, (float)bcd32(data + 8) / 100, (float)bcd32(data + 12) / 100);
  }
  std::function<void(float t1, float t2, float t3, float t4)> on_value_;
};

class Mercury200 : public PollingComponent, public uart::UARTDevice {
public:
  Mercury200(uart::UARTComponent *uart, uint32_t address);
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;
  void set_all_commands(bool all_commands) { this->all_commands_ = all_commands; }
  void set_startup_delay(uint32_t startup_delay) { this->startup_delay_ = startup_delay; }
  void set_dir_pin(GPIOPin *pin) { this->dir_pin_ = pin; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->sensor_voltage_ = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { this->sensor_current_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->sensor_power_ = sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { this->sensor_battery_ = sensor; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *sensor) { this->sensor_error_ = sensor; }
  void set_energy_sensor(uint16_t idx, sensor::Sensor *sensor) { this->sensor_energy_[idx] = sensor; }

protected:
  void delay(uint32_t ms) { this->sleep_time_ = millis() + ms; }
  uint16_t crc16(const uint8_t *data, uint16_t len);
  std::vector<Command *> commands_;

private:
  uint32_t address_, startup_delay_{0}, phase_{0};
  unsigned long sleep_time_{0}, wait_time_{0};
  uint8_t tx_buffer_[TX_BUFFER_SIZE], rx_buffer_[RX_BUFFER_SIZE];
  uint16_t rx_bytes_needed_{0}, rx_bytes_received_{0};
  bool all_commands_, error_{false};
  GPIOPin *dir_pin_{nullptr};
  binary_sensor::BinarySensor *sensor_error_{nullptr};
  sensor::Sensor *sensor_voltage_{nullptr}, *sensor_current_{nullptr}, *sensor_power_{nullptr}, *sensor_battery_{nullptr};
  sensor::Sensor *sensor_energy_[MAX_TARIFF_COUNT];
};

} // namespace mercury200
} // namespace esphome
