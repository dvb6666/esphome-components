#include "fingerprint_sfm.h"

namespace esphome {
namespace fingerprint_sfm {

#define POWER_ON_DELAY 200
#define DIR_PIN_DELAY 5
#define LOG_WAIT_INTERVAL 1000
#define CHUNK_SIZE 40

static const char *TAG = "sfm_v1_7";
static const char *DIGITS = "0123456789ABCDEF";

void SfmComponent::start_scan() {
  ESP_LOGD(TAG, "Start scan batch");
  if (this->finger_scan_start_callback_.size() > 0) {
    ESP_LOGV(TAG, "Executing on_finger_scan_start");
    this->finger_scan_start_callback_.call();
  } else {
    ESP_LOGV(TAG, "No callback on_finger_scan_start");
  }
  ESP_LOGD(TAG, "Add to queue fingerprint scan");
  this->commands_queue_.push(make_unique<SfmCommand>(0x0C));
}

void SfmComponent::start_register(uint16_t finger_id, uint8_t role, uint32_t delay) {
  ESP_LOGD(TAG, "Start register batch: UID %d (0x%04X), Role %d, delay %d", finger_id, finger_id, role, delay);
  if (this->register_start_callback_.size() > 0) {
    ESP_LOGV(TAG, "Executing on_register_start(1)");
    this->register_start_callback_.call(1);
  } else {
    ESP_LOGV(TAG, "No callback on_register_start");
  }
  ESP_LOGD(TAG, "Add to queue 1st register step: UID %d (0x%04X), Role %d, delay %d", finger_id, finger_id, role, delay);
  this->commands_queue_.push(make_unique<SfmCommand>(0x01, (finger_id >> 8) & 0xFF, finger_id & 0xFF, role & 0x03, delay));
}

void SfmComponent::set_color(SfmColor start, SfmColor end, uint16_t period, uint32_t delay) {
  ESP_LOGD(TAG, "Add to queue setting color: start %d, end %d, period %dms, delay %dms", start, end, period, delay);
  period /= 10;
  if (period < 30)
    period = 30;
  if (period > 200)
    period = 200;
  this->commands_queue_.push(make_unique<SfmCommand>(0xC3, start, end, (uint8_t)period, delay));
}

void SfmComponent::delete_one(uint16_t finger_id) {
  ESP_LOGD(TAG, "Add to queue deleting fingerprint: UID %d (0x%04X)", finger_id, finger_id);
  this->commands_queue_.push(make_unique<SfmCommand>(0x04, (finger_id >> 8) & 0xFF, finger_id & 0xFF));
  get_fingerprints_count();
}

void SfmComponent::delete_all() {
  ESP_LOGD(TAG, "Add to queue deleting all fingerprints");
  this->commands_queue_.push(make_unique<SfmCommand>(0x05));
  get_fingerprints_count();
}

void SfmComponent::get_fingerprints_count() {
  ESP_LOGD(TAG, "Add to queue getting fingerprints count");
  this->commands_queue_.push(make_unique<SfmCommand>(0x09));
}

void SfmComponent::get_module_number() {
  ESP_LOGD(TAG, "Add to queue getting module ID number");
  this->commands_queue_.push(make_unique<SfmCommand>(0x60));
}

void SfmComponent::setup() {
  this->running_ = false;
  this->last_touch_state_ = false;
  // setup all pins
  this->sensor_power_pin_->setup();
  this->sensor_power_pin_->digital_write(this->idle_period_to_sleep_ms_ == 0);
  if (this->sensing_pin_)
    this->sensing_pin_->setup();
  if (this->dir_pin_) {
    this->dir_pin_->setup();
    this->dir_pin_->digital_write(false);
  }
  // read old unknown/unused data
  while (this->available())
    this->read();
  // request module ID and fingerprints count on start
  get_module_number();
  get_fingerprints_count();
}

void SfmComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SFM V1.7: %s", this->module_id_.c_str());
  LOG_PIN("  Sensor Power Pin: ", this->sensor_power_pin_);
  if (this->sensing_pin_) {
    LOG_PIN("  Sensing (IRQ) Pin: ", this->sensing_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Sensing (IRQ) Pin: None");
  }
  if (this->idle_period_to_sleep_ms_ > 0) {
    ESP_LOGCONFIG(TAG, "  Idle Period to Sleep: %dms", this->idle_period_to_sleep_ms_);
  } else {
    ESP_LOGCONFIG(TAG, "  Idle Period to Sleep: Never");
  }
  if (this->dir_pin_) {
    LOG_PIN("  Direction Pin: ", this->dir_pin_);
  }
}

void SfmComponent::loop() {
  if (this->sleep_time_ != 0 && this->sleep_time_ > millis())
    return;
  else if (this->sleep_time_ != 0)
    this->sleep_time_ = 0;

  if (!this->commands_queue_.empty()) {
    if (!this->running_) {
      this->running_ = true;
      this->error_ = false;
      this->phase_ = 0;
      // power-on device
      if (this->idle_period_to_sleep_ms_ > 0) {
        ESP_LOGV(TAG, "Set sensor_power_pin to state 'power-on'");
        this->sensor_power_pin_->digital_write(true);
        delay(POWER_ON_DELAY);
        return;
      }
    }

    auto &command = this->commands_queue_.front();
    if (command == nullptr || this->process_command(command.get())) {
      this->commands_queue_.pop();
      this->phase_ = 0;
      // if all commands completed sleep before power-off sensor
      if (this->commands_queue_.empty() && this->idle_period_to_sleep_ms_ > 0) {
        ESP_LOGV(TAG, "Delay %dms before setting sensor_power_pin to state 'power-off'", this->idle_period_to_sleep_ms_);
        delay(this->idle_period_to_sleep_ms_);
      }
    }

  } else if (this->running_ && this->commands_queue_.empty()) {
    this->running_ = false;
    // power-off device
    if (this->idle_period_to_sleep_ms_ > 0) {
      ESP_LOGV(TAG, "Set sensor_power_pin to state 'power-off'");
      this->sensor_power_pin_->digital_write(false);
      delay(POWER_ON_DELAY);
    }
    if (this->error_sensor_)
      this->error_sensor_->publish_state(this->error_);

  } else if (this->sensing_pin_ && !this->last_touch_state_ && this->sensing_pin_->digital_read()) {
    ESP_LOGD(TAG, "Sensor touched!");
    this->last_touch_state_ = true;
    this->start_scan();

  } else if (this->sensing_pin_ && this->last_touch_state_ && !this->sensing_pin_->digital_read()) {
    ESP_LOGD(TAG, "Sensor released!");
    this->last_touch_state_ = false;
  }
}

bool SfmComponent::process_command(SfmCommand *command) {
  switch (this->phase_) {
  case 1: {
    // read old unknown/unused data
    uint16_t unused_bytes;
    for (unused_bytes = 0; unused_bytes < CHUNK_SIZE && this->available(); unused_bytes++)
      this->read();
    if (unused_bytes > 0) {
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: received %d unknown/unused bytes", command->code, this->phase_, unused_bytes);
      return false;
    }
    // prepare data to send
    this->tx_buffer_[0] = this->tx_buffer_[7] = 0xF5;
    this->tx_buffer_[1] = command->code;
    this->tx_buffer_[2] = command->p1;
    this->tx_buffer_[3] = command->p2;
    this->tx_buffer_[4] = command->p3;
    this->tx_buffer_[5] = this->tx_buffer_[6] = 0x00;
    for (uint8_t i = 1; i <= 5; i++)
      this->tx_buffer_[6] ^= this->tx_buffer_[i];
    // set dir_pin to state SEND before sending command
    if (this->dir_pin_) {
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: set dir_pin to state 'send'", command->code, this->phase_);
      this->dir_pin_->digital_write(true);
      delay(DIR_PIN_DELAY);
    }
  } break;

  case 2: {
    // sending command data
    ESP_LOGV(TAG, "Command 0x%02X, phase %d: sending %d bytes", command->code, this->phase_, COMMAND_SIZE);
    this->write_array(this->tx_buffer_, COMMAND_SIZE);
    this->flush();
    this->package_wainting_ = false;
    this->rx_bytes_needed_ = COMMAND_SIZE;
    this->rx_bytes_received_ = 0;
    this->wait_time_ = millis() + 9000; // timeout N milliseconds
  } break;

  case 3: {
    // set dir_pin to state RECV after command sent
    if (this->dir_pin_) {
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: set dir_pin to state 'recv'", command->code, this->phase_);
      this->dir_pin_->digital_write(false);
      delay(DIR_PIN_DELAY);
    }
  } break;

  case 4: {
    // receiving packet
    for (uint16_t i = 0; i < CHUNK_SIZE && this->rx_bytes_received_ < this->rx_bytes_needed_ && this->available(); i++) {
      uint8_t c = this->read();
      if (this->rx_bytes_received_ == 0 && c != 0xF5) {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: skip unknown first byte 0x%02X", command->code, this->phase_, c);
        continue;
      }
      this->rx_buffer_[this->rx_bytes_received_++] = c;
    }
    // wait complete packet?
    if (this->rx_bytes_received_ < this->rx_bytes_needed_) {
      unsigned long current_time = millis();
      if (this->wait_time_ < current_time) {
        ESP_LOGD(TAG, "Command 0x%02X, phase %d: timed out!", command->code, this->phase_);
        return process_error(command);
      } else {
        if (this->log_time_ < current_time) {
          ESP_LOGV(TAG, "Command 0x%02X, phase %d: waiting next %d bytes", command->code, this->phase_,
                   this->rx_bytes_needed_ - this->rx_bytes_received_);
          this->log_time_ = current_time + LOG_WAIT_INTERVAL; // next log in LOG_WAIT_INTERVAL ms
        }
        return false;
      }
    } else {
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: received %d bytes", command->code, this->phase_, this->rx_bytes_received_);
    }
  } break;

  case 5: {
    // validating packet
    if (this->rx_buffer_[this->rx_bytes_needed_ - 1] != 0xF5) {
      ESP_LOGW(TAG, "Command 0x%02X, phase %d: wrong last byte (0x%02X instead of 0xF5)", command->code, this->phase_,
               this->rx_buffer_[this->rx_bytes_needed_ - 1]);
      return process_error(command);
    }
    // checksum [1..-2] bytes
    uint8_t csum = 0;
    for (uint8_t i = 1; i < this->rx_bytes_needed_ - 2; i++)
      csum ^= this->rx_buffer_[i];
    if (csum != this->rx_buffer_[this->rx_bytes_needed_ - 2]) {
      ESP_LOGW(TAG, "Command 0x%02X, phase %d: wrong checksum (0x%02X instead of 0x%02X)", command->code, this->phase_,
               this->rx_buffer_[this->rx_bytes_needed_ - 2], csum);
      return process_error(command);
    }

    // for package skip other validation
    if (this->package_wainting_) {
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: package validation Ok", command->code, this->phase_);
      this->phase_ = 7;
      return false;
    }

    // command header validation
    uint8_t q3 = this->rx_buffer_[4];
    if (command->code != this->rx_buffer_[1]) {
      ESP_LOGW(TAG, "Command 0x%02X, phase %d: got another command response (0x%02X instead of 0x%02X)", command->code, this->phase_,
               this->rx_buffer_[1], command->code);
      return process_error(command);
    } else if (!(q3 == ACK_SUCCESS || (this->rx_buffer_[1] == 0x0C && q3 >= 1 && q3 <= 3))) {
      ESP_LOGW(TAG, "Command 0x%02X, phase %d: got error code 0x%02X", command->code, this->phase_, q3);
      return process_error(command, q3);
    }
    ESP_LOGV(TAG, "Command 0x%02X, phase %d: header validation Ok", command->code, this->phase_);
  } break;

  case 6: {
    // response processing
    uint8_t q1 = this->rx_buffer_[2];
    uint8_t q2 = this->rx_buffer_[3];
    uint8_t q3 = this->rx_buffer_[4];
    switch (command->code) {
    case 0x01:
    case 0x02:
    case 0x03: {
      uint8_t next_step = command->code;
      uint16_t finger_id = (q1 << 8) | q2;
      if (command->code == 0x03) {
        ESP_LOGD(TAG, "Successfully register: UID %d (0x%04X)", finger_id, finger_id);
        if (this->last_finger_id_sensor_)
          this->last_finger_id_sensor_->publish_state(finger_id);
      }
      if (this->register_done_callback_.size() > 0) {
        ESP_LOGV(TAG, "Executing on_register_done(%d)", next_step);
        this->register_done_callback_.call(next_step, finger_id);
      } else {
        ESP_LOGV(TAG, "No callback on_register_done");
      }
    } break;

    case 0x04:
    case 0x05: {
      ESP_LOGD(TAG, "Successfully deleted");
    } break;

    case 0x09: {
      uint16_t count = (q1 << 8) | q2;
      ESP_LOGD(TAG, "Count: %d", count);
      if (this->fingerprint_count_sensor_)
        this->fingerprint_count_sensor_->publish_state(count);
    } break;

    case 0x0C: {
      uint16_t finger_id = (q1 << 8) | q2;
      if (finger_id == 0) {
        ESP_LOGD(TAG, "Not found");
        if (this->finger_scan_unmatched_callback_.size() > 0) {
          ESP_LOGV(TAG, "Executing on_finger_scan_unmatched");
          this->finger_scan_unmatched_callback_.call();
        } else {
          ESP_LOGV(TAG, "No callback on_finger_scan_unmatched");
        }
      } else {
        ESP_LOGD(TAG, "Successfully found: UID %d (0x%04X), Role %d", finger_id, finger_id, q3);
        if (this->last_finger_id_sensor_)
          this->last_finger_id_sensor_->publish_state(finger_id);
        if (this->finger_scan_matched_callback_.size() > 0) {
          ESP_LOGV(TAG, "Executing on_finger_scan_matched(%d, %d)", finger_id, q3);
          this->finger_scan_matched_callback_.call(finger_id, q3);
        } else {
          ESP_LOGV(TAG, "No callback on_finger_scan_matched");
        }
      }
    } break;

    case 0x60: {
      ESP_LOGV(TAG, "Received get_module_number header. Package is needed");
      this->package_wainting_ = true;
      this->rx_bytes_needed_ = PACKAGE_MODULE_ID_SIZE;
      this->rx_bytes_received_ = 0;
      this->phase_ = 4;
      return false;
    } break;
    }
  } break;

  case 7: {
    // package data
    if (this->package_wainting_ && command->code == 0x60) {
      for (uint16_t i = 0; i < MODULE_ID_SIZE; i++) {
        uint8_t c = this->rx_buffer_[1 + i];
        this->module_id_[2 * i] = DIGITS[(c >> 4) & 0x0f];
        this->module_id_[2 * i + 1] = DIGITS[c & 0x0f];
      }
      ESP_LOGD(TAG, "Module number: %s", this->module_id_.c_str());
    }
  } break;

  case 8: {
    // delay after command completed
    if (command->delay > 0) {
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: delay %dms after command completed", command->code, this->phase_, command->delay);
      delay(command->delay);
    }
  } break;

  case 9: {
    // next steps for multi-steps commands
    if (command->code == 0x01 || command->code == 0x02) {
      uint8_t next_step = command->code + 1;
      if (this->register_start_callback_.size() > 0) {
        ESP_LOGV(TAG, "Executing on_register_start(%d)", next_step);
        this->register_start_callback_.call(next_step);
      } else {
        ESP_LOGV(TAG, "No callback on_register_start");
      }
      ESP_LOGD(TAG, "Add to queue next %s register step", next_step == 2 ? "2nd" : "3rd");
      this->commands_queue_.push(make_unique<SfmCommand>(next_step, 0, 0, 0, next_step < 3 ? command->delay : 0));
    }
  } break;

  case 10:
    // final phase
    return true;
  }

  this->phase_++;

  return false;
}

bool SfmComponent::process_error(SfmCommand *command, uint8_t error_code) {
  switch (command->code) {
  case 0x01:
  case 0x02:
  case 0x03: {
    if (this->register_failed_callback_.size() > 0) {
      ESP_LOGV(TAG, "Executing on_register_failed(%d)", error_code);
      this->register_failed_callback_.call(error_code);
    } else {
      ESP_LOGV(TAG, "No callback on_register_failed");
    }
  } break;
  case 0x0C: {
    // TODO need confirm from Testers
    if (1 == 0) { // error_code == ACK_TIMEOUT || error_code == ACK_BREAK)
      if (this->finger_scan_misplaced_callback_.size() > 0) {
        ESP_LOGV(TAG, "Executing on_finger_scan_misplaced");
        this->finger_scan_misplaced_callback_.call();
      } else {
        ESP_LOGV(TAG, "No callback on_finger_scan_misplaced");
      }
    } else {
      if (this->finger_scan_failed_callback_.size() > 0) {
        ESP_LOGV(TAG, "Executing on_finger_scan_failed(%d)", error_code);
        this->finger_scan_failed_callback_.call(error_code);
      } else {
        ESP_LOGV(TAG, "No callback on_finger_scan_failed");
      }
    }
  } break;
  default:
    ESP_LOGV(TAG, "Command %02x has no callback on_failed", command->code);
  }
  return true;
}

} // namespace fingerprint_sfm
} // namespace esphome
