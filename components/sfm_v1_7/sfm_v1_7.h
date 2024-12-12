#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace sfm_v1_7 {

#define COMMAND_SIZE 8

enum SFM_Color : uint8_t {
  YELLOW = 0x01,
  PURPLE = 0x02,
  RED = 0x03,
  CYAN = 0x04,
  GREEN = 0x05,
  BLUE = 0x06,
  OFF = 0x07,
};

struct Command { uint8_t code; uint8_t p1; uint8_t p2; uint8_t p3; uint16_t delay; bool run_always; };
struct CommandBatch {
//public:
  //CommandBatch(bool enable_, std::vector<Command*> const &commands_) : need_enable(enable_), commands(commands_) {};
  //bool need_enable;
  std::vector<Command*> commands;
};
//uint16_t length{0}; Command **commands; };

class SFM_v1_7 : public Component, public uart::UARTDevice {
public:
  SFM_v1_7(uart::UARTComponent *uart, InternalGPIOPin *vcc_pin, bool vcc_always_on)
    : uart::UARTDevice(uart), vcc_pin_(vcc_pin), vcc_always_on_(vcc_always_on) {};
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }
  void set_dir_pin(InternalGPIOPin *pin) { this->dir_pin_ = pin; }
  void set_error_sensor(binary_sensor::BinarySensor *sensor) { this->sensor_error_ = sensor; }
  void start_scan();
  void start_register(uint16_t uid = 0, uint8_t role = 3);
  void set_color(SFM_Color start, SFM_Color end = SFM_Color::OFF, uint16_t period = 500, uint16_t delay = 500);

protected:
  void delay(uint32_t ms) { this->sleep_time_ = millis() + ms; }

private:
  uint16_t phase_{0};
  unsigned long sleep_time_{0}, wait_time_{0}, log_time_{0};
  CommandBatch *batch_{nullptr};
  uint16_t rx_bytes_needed_{0}, rx_bytes_received_{0};
  uint8_t tx_buffer_[COMMAND_SIZE], rx_buffer_[COMMAND_SIZE];
  bool vcc_always_on_, error_{false}, last_touch_state_{false};
  InternalGPIOPin *dir_pin_{nullptr}, *irq_pin_{nullptr}, *vcc_pin_;
  binary_sensor::BinarySensor *sensor_error_{nullptr};
};

template <typename... Ts> class SFM_SetColorAction : public Action<Ts...> {
public:
  SFM_SetColorAction(SFM_v1_7 *parent, SFM_Color start, SFM_Color end, uint16_t period, uint16_t delay)
    : parent_(parent), start_(start), end_(end), period_(period), delay_(delay) {}
  void play(Ts... x) override { parent_->set_color(start_, end_, period_, delay_); }
private:
  SFM_v1_7 *parent_;
  SFM_Color start_, end_;
  uint16_t period_, delay_;
};

} // namespace sfm_v1_7
} // namespace esphome
