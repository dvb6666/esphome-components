#include "aqua_watchman.h"
#include "esphome/core/log.h"

namespace esphome {
namespace aqua_watchman {

using namespace esphome::valve;

static const char *const TAG = "aqua_watchman.valve";

#define COMMAND(cmd) (cmd == OPEN ? "OPEN" : cmd == ALARM ? "ALARM" : "CLOSE")
#define PIN_PTR(cmd) \
  (cmd == OPEN ? this->open_pin_ : cmd == ALARM && this->alarm_pin_ ? this->alarm_pin_ : this->close_pin_)
#define PIN_NAME(cmd) (cmd == OPEN ? "OPEN" : cmd == ALARM && this->alarm_pin_ ? "ALARM" : "CLOSE")

AquaWatchmanValve::AquaWatchmanValve(InternalGPIOPin *close_pin, InternalGPIOPin *open_pin)
    : close_pin_(close_pin), open_pin_(open_pin) {
  this->traits_.set_is_assumed_state(true);
  this->traits_.set_supports_position(false);
  this->traits_.set_supports_stop(false);
  this->traits_.set_supports_toggle(false);
}

// #define OPEN_PIN 5
void AquaWatchmanValve::setup() {
  ESP_LOGCONFIG(TAG, "Setting up...");
  this->close_pin_->setup();
  this->open_pin_->setup();
  if (this->alarm_pin_)
    this->alarm_pin_->setup();
  this->position = 0.5f;
  this->current_operation = VALVE_OPERATION_IDLE;
  this->publish_state(false);
}

void AquaWatchmanValve::dump_config() {
  LOG_VALVE("", "AquaWatchman Valve", this);
  LOG_PIN("  Close Pin: ", this->close_pin_);
  LOG_PIN("  Open Pin: ", this->open_pin_);
  if (this->alarm_pin_)
    LOG_PIN("  Alarm Pin: ", this->alarm_pin_);
}

void AquaWatchmanValve::loop() {
  if (this->sleep_time_ != 0 && this->sleep_time_ > millis())
    return;
  else if (this->sleep_time_ != 0)
    this->sleep_time_ = 0;

  // process OPEN/CLOSE/ALARM commands
  if (!this->queue_.empty()) {
    auto &command = this->queue_.front();
    auto pin = PIN_PTR(command->code);
    switch (++this->phase_) {
      case 1: {
        ESP_LOGD(TAG, "Executing command %s (0x%02x)", COMMAND(command->code), command->code);
        this->current_operation = (command->code == OPEN ? VALVE_OPERATION_OPENING : VALVE_OPERATION_CLOSING);
        this->position = (command->code == OPEN ? 0.7f : 0.3f);
        this->publish_state(false);
      } break;

      case 2: {
        if (!command->silent) {
          ESP_LOGV(TAG, "Setting %s pin to ON", PIN_NAME(command->code));
          // pin->digital_write(pin->is_inverted());
          pin->pin_mode(gpio::FLAG_OUTPUT);
          pin->digital_write(pin->is_inverted());
        }
        delay(400);
      } break;

      case 3: {
        if (!command->silent) {
          ESP_LOGV(TAG, "Setting %s pin to OFF", PIN_NAME(command->code));
          pin->digital_write(!pin->is_inverted());
        }
        delay(100);
      } break;

      case 4: {
        if (!command->silent) {
          pin->pin_mode(gpio::FLAG_INPUT);
        }
        delay(3000);  // TODO extract to yaml-config param
      } break;

      case 5: {
        ESP_LOGD(TAG, "Executed command %s (0x%02x)", COMMAND(command->code), command->code);
        this->position = (command->code == OPEN ? VALVE_OPEN : VALVE_CLOSED);
        this->current_operation = VALVE_OPERATION_IDLE;
        this->publish_state(false);
        this->queue_.pop();
        this->phase_ = 0;
        delay(100);
      } break;
    }
    return;
  }

  // skip buttons state check when option "no_buttons_check" set
  if (this->no_buttons_check_)
    return;

  // check if pressed device button OPEN
  bool open = this->open_pin_->digital_read();
  if (this->previous_open_value_ && open) {
    ESP_LOGD(TAG, "Got OPEN command from device");
    this->queue_.push(make_unique<AquaWatchmanCommand>(OPEN, true));
    return;
  } else if (this->previous_open_value_ != open) {
    this->previous_open_value_ = open;
    if (open) {
      ESP_LOGV(TAG, "Got OPEN command from device?");
      delay(10);  // debounce protection
      return;
    }
  }

  // check if pressed device button CLOSE
  bool close = this->close_pin_->digital_read();
  if (this->previous_close_value_ && close) {
    ESP_LOGD(TAG, "Got CLOSE command from device");
    this->queue_.push(make_unique<AquaWatchmanCommand>(CLOSE, true));
    return;
  } else if (this->previous_close_value_ != close) {
    this->previous_close_value_ = close;
    if (close) {
      ESP_LOGV(TAG, "Got CLOSE command from device?");
      delay(10);  // debounce protection
      return;
    }
  }
}

void AquaWatchmanValve::control(const ValveCall &call) {
  float pos;
  if (call.get_position().has_value() && (((pos = *call.get_position()) == VALVE_OPEN) || (pos == VALVE_CLOSED))) {
    if (!this->queue_.empty())
      ESP_LOGD(TAG, "Previous commands not completed");
    auto code = (pos == VALVE_OPEN ? OPEN : this->alarm_ ? ALARM : CLOSE);
    this->alarm_ = false;
    this->queue_.push(make_unique<AquaWatchmanCommand>(code, false));
    ESP_LOGV(TAG, "Adding to queue command %s (0x%02x)", COMMAND(code), code);
  } else {
    ESP_LOGV(TAG, "Unknown operation without position");
  }
}

}  // namespace aqua_watchman
}  // namespace esphome
