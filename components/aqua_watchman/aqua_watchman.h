#pragma once
#ifndef AQUA_WATCHMAN_COMPONENTS
#define AQUA_WATCHMAN_COMPONENTS

#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/valve/valve.h"
#include <memory>
#include <queue>

namespace esphome {
namespace aqua_watchman {

enum AquaWatchmanCommandCode : uint8_t { CLOSE = 0x01, OPEN = 0x02, ALARM = 0x03 };

class AquaWatchmanCommand {
 public:
  AquaWatchmanCommand(AquaWatchmanCommandCode code_, bool silent_) : code(code_), silent(silent_) {};
  AquaWatchmanCommandCode code;
  bool silent;
};

class AquaWatchmanValve : public valve::Valve, public Component {
 public:
  explicit AquaWatchmanValve(InternalGPIOPin *close_pin, InternalGPIOPin *open_pin);

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_alarm_pin(InternalGPIOPin *pin) { this->alarm_pin_ = pin; }
  void set_ignore_buttons(bool value) { this->ignore_buttons_ = value; }
  bool alarm_{false};  // TODO bad workaround for alarm action

 protected:
  void control(const valve::ValveCall &call) override;
  valve::ValveTraits get_traits() override { return this->traits_; }
  void delay(uint32_t ms) { this->sleep_time_ = millis() + ms; }

 private:
  InternalGPIOPin *close_pin_, *open_pin_, *alarm_pin_{nullptr};
  valve::ValveTraits traits_{};
  std::queue<std::unique_ptr<AquaWatchmanCommand>> queue_;
  unsigned long sleep_time_{0};
  uint16_t phase_{0};
  bool previous_close_value_{false}, previous_open_value_{false}, ignore_buttons_{false};
};

template<typename... Ts> class AquaWatchmanAlarmAction : public Action<Ts...> {
 public:
  explicit AquaWatchmanAlarmAction(AquaWatchmanValve *valve) : valve_(valve) {}

  void play(Ts... x) override {
    this->valve_->alarm_ = true;
    this->valve_->make_call().set_command_close().perform();
  }

 protected:
  AquaWatchmanValve *valve_;
};

}  // namespace aqua_watchman
}  // namespace esphome

#endif
