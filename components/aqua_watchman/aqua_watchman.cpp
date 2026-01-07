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

void AquaWatchmanValve::setup() {
  ESP_LOGCONFIG(TAG, "Setting up...");
  this->close_pin_->setup();
  this->open_pin_->setup();
  if (this->alarm_pin_) {
    this->alarm_pin_->setup();
    this->floor_cleaning_state_ = this->alarm_pin_->digital_read() == this->alarm_pin_->is_inverted();
    if (this->floor_cleaning_sensor_)
      this->floor_cleaning_sensor_->publish_state(this->floor_cleaning_state_);
  }
  if (this->power_pin_) {
    this->power_pin_->setup();
    this->power_state_ = this->power_pin_->digital_read() == this->power_pin_->is_inverted();
    if (this->power_sensor_)
      this->power_sensor_->publish_state(this->power_state_);
  }

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
  if (this->power_pin_)
    LOG_PIN("  Power Pin: ", this->power_pin_);
  if (this->floor_cleaning_sensor_)
    LOG_BINARY_SENSOR("  ", "Floor Cleaning Sensor: ", this->floor_cleaning_sensor_);
  if (this->power_sensor_)
    LOG_BINARY_SENSOR("  ", "Power Sensor: ", this->power_sensor_);
  ESP_LOGCONFIG(TAG, "  Ignore Buttons: %s", YESNO(this->ignore_buttons_));
}

void AquaWatchmanValve::loop() {
  if (this->sleep_time_ != 0 && this->sleep_time_ > millis())
    return;
  else if (this->sleep_time_ != 0)
    this->sleep_time_ = 0;

  // process OPEN/CLOSE/ALARM commands
  if (!this->queue_.empty()) {
    auto &command = this->queue_.front();
    // check for NULL
    if (command == nullptr) {
      ESP_LOGW(TAG, "Null command in queue");
      this->queue_.pop();
      return;
    }
    // define pin and process all phases
    auto pin = PIN_PTR(command->code);
    switch (++this->phase_) {
      case 1: {
        ESP_LOGD(TAG, "Executing command %s (0x%02x)", COMMAND(command->code), command->code);
        this->current_operation = (command->code == OPEN ? VALVE_OPERATION_OPENING : VALVE_OPERATION_CLOSING);
        this->position = (command->code == OPEN ? 0.7f : 0.3f);
        this->publish_state(false);
        this->replay_counter_ = 0;
      } break;

      case 2: {
        if (!command->silent) {
          ESP_LOGV(TAG, "Setting %s pin (%d) to HIGH (OFF)", PIN_NAME(command->code), pin->get_pin());
          pin->pin_mode(gpio::FLAG_OUTPUT);
          pin->digital_write(!pin->is_inverted());
        }
        delay(100);
      } break;

      case 3: {
        if (!command->silent) {
          ESP_LOGV(TAG, "Setting %s pin (%d) to LOW (ON)", PIN_NAME(command->code), pin->get_pin());
          pin->digital_write(pin->is_inverted());
          if (command->code == ALARM && (++this->replay_counter_ < 2)) {
            ESP_LOGV(TAG, "Replay alarm command");
            this->phase_ = 1;
          }
        }
        delay(command->code == ALARM ? 900 : 400);

      } break;

      case 4: {
        if (!command->silent) {
          ESP_LOGV(TAG, "Setting %s pin (%d) to HIGH (OFF)", PIN_NAME(command->code), pin->get_pin());
          pin->digital_write(!pin->is_inverted());
        }
        delay(100);
      } break;

      case 5: {
        if (!command->silent) {
          pin->pin_mode(gpio::FLAG_INPUT);
        }
        delay(2500);  // TODO extract to yaml-config param
      } break;

      case 6: {
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

  // check power pin state only when no command executing (because on Opening command it changes its state to LOW for short time)
  if (this->queue_.empty() && this->power_pin_ && this->power_sensor_) {
    this->power_state_ = this->power_pin_->digital_read();// == this->power_pin_->is_inverted();
    if (this->power_state_ != this->power_sensor_->state) {
      ESP_LOGV(TAG, "Power state changed to %s", this->power_state_ ? "true" : "false");
      this->power_sensor_->publish_state(this->power_state_);
    }
  }

  // skip buttons state check when option "ignore_buttons" is set
  if (this->ignore_buttons_)
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

  // check alarm pin
  if (this->alarm_pin_ && this->floor_cleaning_sensor_) {
    this->floor_cleaning_state_ = this->alarm_pin_->digital_read() == this->alarm_pin_->is_inverted();
    if (this->floor_cleaning_state_ != this->floor_cleaning_sensor_->state) {
      ESP_LOGV(TAG, "Alarm pin state changed to %s", this->floor_cleaning_state_ ? "true" : "false");
      this->floor_cleaning_sensor_->publish_state(this->floor_cleaning_state_);
    }
  }
}

void AquaWatchmanValve::send_alarm_command() {
  this->alarm_ = true;
  if (!this->alarm_pin_)
    ESP_LOGW(TAG, "Alarm pin not set!");
  else if (this->floor_cleaning_state_) {
    ESP_LOGW(TAG, "Can't alarm coz floor mode enabled!");
    this->alarm_ = false;
  }
  this->make_call().set_command_close().perform();
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
