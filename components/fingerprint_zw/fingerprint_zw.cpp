#include "fingerprint_zw.h"

namespace esphome {
namespace fingerprint_zw {

#define POWER_ON_DELAY 200
#define DIR_PIN_DELAY 5
#define LOG_WAIT_INTERVAL 1000
#define CHUNK_SIZE 64
#define READ_TIMEOUT 9000

static const char *TAG = "zw111";
static const char *DIGITS = "0123456789ABCDEF";

void ZwComponent::start_scan() {
  ESP_LOGD(TAG, "Start scan batch");
  if (this->finger_scan_start_callback_.size() > 0) {
    ESP_LOGV(TAG, "Executing on_finger_scan_start");
    this->finger_scan_start_callback_.call();
  } else {
    ESP_LOGV(TAG, "No callback on_finger_scan_start");
  }
  ESP_LOGD(TAG, "Add to queue getting image");
  this->commands_queue_.push(make_unique<ZwCommandGetImage>());
}

void ZwComponent::start_register(uint16_t finger_id, uint8_t role, uint32_t delay) {
  // ESP_LOGD(TAG, "Start register batch: UID %d (0x%04X), Role %d, delay %d", finger_id, finger_id, role, delay);
}

void ZwComponent::aura_led_control(ZwAuraLEDState state, ZwAuraLEDColor color, uint8_t count) {
  ESP_LOGD(TAG, "Add to queue setting color: state %d, color %d, count %d", state, color, count);
  this->commands_queue_.push(make_unique<ZwCommandControlBLN>(state, color, color, count));
}

void ZwComponent::delete_one(uint16_t finger_id, uint16_t number) {
  ESP_LOGD(TAG, "Add to queue deleting %d fingerprints staring from ID %d ", number, finger_id);
  this->commands_queue_.push(make_unique<ZwCommandDeleteOne>(finger_id, number));
  get_fingerprints_count();
}

void ZwComponent::delete_all() {
  ESP_LOGD(TAG, "Add to queue deleting all fingerprints");
  this->commands_queue_.push(make_unique<ZwCommandDeleteAll>());
  get_fingerprints_count();
}

void ZwComponent::get_module_parameters() {
  ESP_LOGD(TAG, "Add to queue getting module parameters");
  // this->commands_queue_.push(make_unique<ZwCommandCheckSensor>());
  this->commands_queue_.push(make_unique<ZwCommandReadSysPara>());
}

void ZwComponent::get_flash_parameters() {
  ESP_LOGD(TAG, "Add to queue getting flash parameters");
  this->commands_queue_.push(make_unique<ZwCommandInfPage>());
}

void ZwComponent::get_serial_number() {
  ESP_LOGD(TAG, "Add to queue getting module serial number");
  this->commands_queue_.push(make_unique<ZwCommandGetChipSN>());
}

void ZwComponent::get_fingerprints_count() {
  ESP_LOGD(TAG, "Add to queue getting fingerprints count");
  this->commands_queue_.push(make_unique<ZwCommandValidTempleteNum>());
}

void ZwComponent::setup() {
  this->running_ = false;
  this->last_touch_state_ = false;
  // setup all pins
  if (this->sensor_power_pin_) {
    this->sensor_power_pin_->setup();
    this->sensor_power_pin_->digital_write(this->idle_period_to_sleep_ms_ == 0);
  }
  if (this->sensing_pin_)
    this->sensing_pin_->setup();
  // read old unknown/unused data
  while (this->available())
    this->read();
  // request module ID and fingerprints count on start
  get_serial_number();
  get_module_parameters();
  get_fingerprints_count();
}

void ZwComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ZW111 Serial Number \"%s\", Address 0x%08X", this->module_id_, this->address_);
  if (this->sensor_power_pin_) {
    LOG_PIN("  Sensor Power Pin: ", this->sensor_power_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Sensor Power Pin: None");
  }
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
  if (this->error_sensor_)
    LOG_BINARY_SENSOR("  ", "Error Sensor: ", this->error_sensor_);
  if (this->fingerprint_count_sensor_)
    LOG_SENSOR("  ", "Fingerprints Count Sensor: ", this->fingerprint_count_sensor_);
  if (this->last_finger_id_sensor_)
    LOG_SENSOR("  ", "Last Fingerprint ID Sensor: ", this->last_finger_id_sensor_);
}

void ZwComponent::loop() {
  if (this->sleep_time_ != 0 && this->sleep_time_ > millis())
    return;
  else if (this->sleep_time_ != 0)
    this->sleep_time_ = 0;

  if (!this->commands_queue_.empty()) {
    if (!this->running_) {
      this->running_ = true;
      this->error_ = false;
      this->phase_ = 0;
      this->retry_count_ = 0;
      // power-on device
      if (this->idle_period_to_sleep_ms_ > 0 && this->sensor_power_pin_) {
        ESP_LOGV(TAG, "Set sensor_power_pin(%d) to state 'power-on'", this->sensor_power_pin_->get_pin());
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
      if (this->commands_queue_.empty() && this->idle_period_to_sleep_ms_ > 0 && this->sensor_power_pin_) {
        ESP_LOGV(TAG, "Delay %dms before setting sensor_power_pin(%d) to state 'power-off'",
                 this->idle_period_to_sleep_ms_, this->sensor_power_pin_->get_pin());
        delay(this->idle_period_to_sleep_ms_);
      }
    }

  } else if (this->running_ && this->commands_queue_.empty()) {
    this->running_ = false;
    // power-off device
    if (this->idle_period_to_sleep_ms_ > 0 && this->sensor_power_pin_) {
      ESP_LOGV(TAG, "Set sensor_power_pin(%d) to state 'power-off'", this->sensor_power_pin_->get_pin());
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

bool ZwComponent::process_command(ZwCommand *command) {
  switch (this->phase_) {
    case 1: {
      // read old unknown/unused data
      uint16_t unused_bytes;
      for (unused_bytes = 0; unused_bytes < CHUNK_SIZE && this->available(); unused_bytes++)
        this->read();
      if (unused_bytes > 0) {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: received %d unknown/unused bytes", command->code, this->phase_,
                 unused_bytes);
        return false;
      }
      // prepare data to send
      this->tx_buffer_[0] = 0xEF;  // header 0xEF01
      this->tx_buffer_[1] = 0x01;
      this->tx_buffer_[2] = 0xFF;  // address 0xFFFFFFFF
      this->tx_buffer_[3] = 0xFF;
      this->tx_buffer_[4] = 0xFF;
      this->tx_buffer_[5] = 0xFF;
      this->tx_buffer_[6] = 0x01;                                       // package ID: command (01)
      this->tx_buffer_[7] = (uint8_t) ((command->length >> 8) & 0xFF);  // length
      this->tx_buffer_[8] = (uint8_t) (command->length & 0xFF);
      this->tx_buffer_[9] = command->code;  // script code
      if (command->length > 3)
        this->tx_buffer_[10] = command->p1;
      if (command->length > 4)
        this->tx_buffer_[11] = command->p2;
      if (command->length > 5)
        this->tx_buffer_[12] = command->p3;
      if (command->length > 6)
        this->tx_buffer_[13] = command->p4;
      // checksum starting package ID
      uint16_t checksum = 0;
      for (uint8_t i = 0; i <= command->length; i++)
        checksum += this->tx_buffer_[i + 6];
      this->tx_buffer_[command->length + 7] = (uint8_t) ((checksum >> 8) & 0xFF);
      this->tx_buffer_[command->length + 8] = (uint8_t) (checksum & 0xFF);
    } break;

    case 2: {
      // sending command data
      uint16_t size = PACKAGE_HEADER_LENGTH + command->length;
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: sending %d bytes", command->code, this->phase_, size);
      this->write_array(this->tx_buffer_, size);
      this->flush();
      this->wait_package_ = false;
      this->rx_bytes_needed_ = PACKAGE_HEADER_LENGTH + command->response_length;
      this->rx_bytes_received_ = 0;
      this->wait_time_ = millis() + READ_TIMEOUT;  // timeout N milliseconds
    } break;

    case 3: {
      // workaround to receive all data
      if (command->packaged)
        delay(300);
    } break;

    // receiving packet
    case 4: {
      // read data from UART with chunks
      for (uint16_t i = 0; i < CHUNK_SIZE && this->rx_bytes_received_ < this->rx_bytes_needed_ && available(); i++) {
        uint8_t c = this->read();
        // package should start from 0xEF, 0x01 bytes
        if (this->rx_bytes_received_ == 0 && c != 0xEF) {
          ESP_LOGV(TAG, "Command 0x%02X, phase %d: skip unknown first byte 0x%02X", command->code, this->phase_, c);
          continue;
        } else if (this->rx_bytes_received_ == 1 && c != 0x01) {
          ESP_LOGV(TAG, "Command 0x%02X, phase %d: skip unknown second byte 0x%02X", command->code, this->phase_, c);
          this->rx_bytes_received_ = 0;
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
            this->log_time_ = current_time + LOG_WAIT_INTERVAL;  // next log in LOG_WAIT_INTERVAL ms
          }
          return false;
        }
      } else {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: received %d bytes", command->code, this->phase_,
                 this->rx_bytes_received_);
      }
      // received data package header?
      if (this->wait_package_ && this->rx_bytes_received_ == PACKAGE_HEADER_LENGTH) {
        uint16_t received_length = ((uint16_t) this->rx_buffer_[7] << 8) + (uint16_t) this->rx_buffer_[8];
        this->rx_bytes_needed_ += received_length;
        if (this->rx_bytes_needed_ > RX_BUFFER_SIZE) {
          ESP_LOGW(TAG, "Command 0x%02X, phase %d: too big package size (%d bytes) requested!", command->code,
                   this->phase_, this->rx_bytes_needed_);
          return process_error(command);
        }
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: package header received, waiting for next %d bytes", command->code,
                 this->phase_, received_length);
        return false;
      }
    } break;

    // validating packet
    case 5: {
      // check package length
      uint16_t received_length = ((uint16_t) this->rx_buffer_[7] << 8) + (uint16_t) this->rx_buffer_[8];
      uint16_t needed_length = this->wait_package_ ? received_length : command->response_length;
      if (needed_length != received_length) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d: wrong packet length (%d instead of %d)", command->code, this->phase_,
                 received_length, needed_length);
        return process_error(command);
      }
      // checksum
      uint16_t checksum =
          ((uint16_t) this->rx_buffer_[received_length + 7] << 8) + (uint16_t) this->rx_buffer_[received_length + 8];
      uint16_t calc_checksum = 0;
      for (uint8_t i = 0; i <= received_length; i++)
        calc_checksum += this->rx_buffer_[i + 6];
      if (calc_checksum != checksum) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d: wrong checksum (0x%04X instead of 0x%04X)", command->code,
                 this->phase_, calc_checksum, checksum);
        // if (!this->wait_package_)  // TODO unknown bad checksum
        return process_error(command);
      }
      // 1st (command repsonse) package should have package ID 0x07
      if (!this->wait_package_ && this->rx_buffer_[6] != 0x07) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d: wrong package ID 0x%02x for command package", command->code,
                 this->phase_, this->rx_buffer_[6]);
        return process_error(command);
      }
      // packaged commands receive confirmation at 1st package
      uint8_t confirmation = this->rx_buffer_[9];
      if (!this->wait_package_ && command->packaged && confirmation != 0x00) {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: got confirmation 0x%02X", command->code, this->phase_, confirmation);
        return process_error(command, confirmation);
      }
      // next (data) packages should have package ID 0x02 or 0x08
      if (this->wait_package_ && this->rx_buffer_[6] != 0x02 && this->rx_buffer_[6] != 0x08) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d: wrong package ID 0x%02x for data package", command->code, this->phase_,
                 this->rx_buffer_[6]);
        return process_error(command);
      }
      // everything OK
      ESP_LOGV(TAG, "Command 0x%02X, phase %d, package ID 0x%02x validation Ok", command->code, this->phase_,
               this->rx_buffer_[6]);
      // copy packet data to buffer
      if (this->wait_package_) {
        uint16_t data_length = received_length - 2;  // data size without two last bytes of checksum
        if (this->data_bytes_received_ + data_length > DATA_BUFFER_SIZE) {
          ESP_LOGW(TAG, "Command 0x%02X, phase %d: too big data size (%d bytes) requested!", command->code,
                   this->phase_, this->data_bytes_received_ + data_length);
          return process_error(command);
        }
        memcpy(this->data_buffer_ + this->data_bytes_received_, &this->rx_buffer_[9], data_length);
        this->data_bytes_received_ += data_length;
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: got next %d data bytes (all %d bytes)", command->code, this->phase_,
                 data_length, this->data_bytes_received_);
      }
      // resume receiving data for packaged commands
      if (command->packaged && (this->rx_buffer_[6] == 0x07 || this->rx_buffer_[6] == 0x02)) {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: waiting next data package after package ID 0x%02x", command->code,
                 this->phase_, this->rx_buffer_[6]);
        if (!this->wait_package_) {
          this->wait_package_ = true;
          this->data_bytes_received_ = 0;
        }
        this->rx_bytes_needed_ = PACKAGE_HEADER_LENGTH;
        this->rx_bytes_received_ = 0;
        this->wait_time_ = millis() + READ_TIMEOUT;
        this->phase_--;  // go to previous phase 'receiving packet'
        return false;
      }
    } break;

    // response processing
    case 6: {
      // check confirmation (error code) for non-packaged commands
      uint8_t confirmation = this->rx_buffer_[9];
      if (!this->wait_package_ && confirmation != 0x00) {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: got confirmation 0x%02X", command->code, this->phase_, confirmation);
        return process_error(command, confirmation);
      }

      // process result
      switch (command->code) {
        case PS_GetImage: {
          ESP_LOGD(TAG, "Add to queue generating fingerprint feature");
          this->commands_queue_.push(make_unique<ZwCommandGenChar>(1));
        } break;

        case PS_GenChar: {
          ESP_LOGD(TAG, "Add to queue searching fingerprint");
          this->commands_queue_.push(make_unique<ZwCommandSearch>(this->capacity_, 1));
        } break;

        case PS_Search: {
          uint16_t finger_id = ((uint16_t) this->rx_buffer_[10] << 8) | this->rx_buffer_[11];
          uint16_t score = ((uint16_t) this->rx_buffer_[12] << 8) | this->rx_buffer_[13];
          ESP_LOGD(TAG, "Successfully found: UID %d (0x%04X), Match Score %d", finger_id, finger_id, score);
          if (this->last_finger_id_sensor_)
            this->last_finger_id_sensor_->publish_state(finger_id);
          if (this->finger_scan_matched_callback_.size() > 0) {
            ESP_LOGV(TAG, "Executing on_finger_scan_matched(%d, %d)", finger_id, score);
            this->finger_scan_matched_callback_.call(finger_id, score);
          } else {
            ESP_LOGV(TAG, "No callback on_finger_scan_matched");
          }
        } break;

        case PS_ReadSysPara: {
          ReadSysParaResponse *params = (ReadSysParaResponse *) &this->rx_buffer_[10];
          this->capacity_ = htons(params->database_size);
          // this->address_ = htonl(params->device_address);
          ESP_LOGD(
              TAG,
              "Params: EnrollTimes=%d, TempSize=%d, DataBaseSize=%d, ScoreLevel=%d, DeviceAddress=0x%08X, BaudRate=%d",
              htons(params->enroll_times), htons(params->temp_size), this->capacity_, htons(params->score_level),
              this->address_, htons(params->baud_rate) * 9600);
          if (this->capacity_sensor_)
            this->capacity_sensor_->publish_state(this->capacity_);
        } break;

        case PS_ReadlNFpage: {
          if (this->data_bytes_received_ < 512) {  // sizeof(ReadInfPageResponse)) {
            ESP_LOGW(TAG, "Command 0x%02X, phase %d: not all data received for NFpage", command->code, this->phase_);
            return process_error(command);
          }
          ReadInfPageResponse *params = (ReadInfPageResponse *) &this->data_buffer_;
          this->capacity_ = htons(params->database_size);
          strncpy(this->product_sn_, params->product_sn, 8);
          strncpy(this->sw_version_, params->sw_version, 8);
          strncpy(this->manufacturer_, params->manufacturer, 8);
          strncpy(this->sensor_name_, params->sensor_name, 8);
          ESP_LOGD(
              TAG,
              "Params: EnrollTimes=%d, TempSize=%d, DataBaseSize=%d, ScoreLevel=%d, DeviceAddress=0x%08X, BaudRate=%d",
              htons(params->enroll_times), htons(params->temp_size), this->capacity_, htons(params->score_level),
              this->address_, htons(params->baud_rate) * 9600);
          ESP_LOGD(
              TAG,
              "Params: ResSpitefullmg=%d, FPSensorPara=%d, Encryption=%d, EnrollLogic=%d, ImageFormat=%d, PortDelay=%d",
              htons(params->prevent_false_fingerprints), htons(params->fp_sensor), htons(params->encryption_level),
              htons(params->enroll_logic), htons(params->image_format), htons(params->serial_port_delay));
          ESP_LOGD(TAG, "ProductSN: \"%s\", SoftwareVersion: \"%s\", Manufacturer: \"%s\", SensorName: \"%s\"",
                   this->product_sn_, this->sw_version_, this->manufacturer_, this->sensor_name_);
          if (this->capacity_sensor_)
            this->capacity_sensor_->publish_state(this->capacity_);
        } break;

        case PS_ValidTempleteNum: {
          uint16_t count = ((uint16_t) this->rx_buffer_[10] << 8) + (uint16_t) this->rx_buffer_[11];
          ESP_LOGD(TAG, "Got fingerprint count: %d", count);
          if (this->fingerprint_count_sensor_)
            this->fingerprint_count_sensor_->publish_state(count);
        } break;

        case PS_GetChipSN: {
          for (uint16_t i = 0; i < SERIAL_NUMBER_SIZE; i++) {
            uint8_t c = this->rx_buffer_[10 + i];
            this->module_id_[2 * i] = DIGITS[(c >> 4) & 0x0F];
            this->module_id_[2 * i + 1] = DIGITS[c & 0x0F];
          }
          ESP_LOGD(TAG, "Module serial number: %s", this->module_id_);
        } break;
      }
    } break;

    case 7: {
      // delay after command completed
      if (command->delay > 0) {
        ESP_LOGV(TAG, "Command 0x%02X, phase %d: delay %dms after command completed", command->code, this->phase_,
                 command->delay);
        delay(command->delay);
      }
    } break;

    case 8: {
      // next steps for multi-steps commands
    } break;

    case 9:
      // final phase
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: command completed", command->code, this->phase_);
      return true;
  }

  this->phase_++;

  return false;
}

bool ZwComponent::process_error(ZwCommand *command, uint8_t error_code) {
  switch (command->code) {
    case PS_GetImage:
    case PS_GenChar:
    case PS_Search: {
      if (error_code == 0x09) {
        ESP_LOGD(TAG, "Not found");
        if (this->finger_scan_unmatched_callback_.size() > 0) {
          ESP_LOGV(TAG, "Executing on_finger_scan_unmatched");
          this->finger_scan_unmatched_callback_.call();
        } else {
          ESP_LOGV(TAG, "No callback on_finger_scan_unmatched");
        }
      } else if (1 == 0) {  // TODO for misplace handling
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

    case PS_ReadlNFpage: {
      if (++this->retry_count_ < 3) {
        ESP_LOGV(TAG, "Error on PS_ReadlNFpage command. Doing retry %d...", this->retry_count_);
        get_flash_parameters();
      }
    } break;

    default:
      ESP_LOGV(TAG, "Command %02x has no callback on_failed", command->code);
  }
  return true;
}

}  // namespace fingerprint_zw
}  // namespace esphome
