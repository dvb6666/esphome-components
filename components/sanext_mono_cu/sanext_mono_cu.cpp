#include "sanext_mono_cu.h"

namespace esphome {
namespace sanext_mono_cu {

#define LOG_WAIT_INTERVAL 1000
#define CHUNK_SIZE 64
#define READ_TIMEOUT 2000

static const char *TAG = "sanext_mono_cu";
//static const char *DIGITS = "0123456789ABCDEF";

void SanextMonoCU::read_meter() {
  ESP_LOGD(TAG, "Add to queue meter reading command");
  this->commands_queue_.push(make_unique<SanextCommandReadMeter>());
}

void SanextMonoCU::setup() {
  // read old unknown/unused data
  while (this->available())
    this->read();
  delay(500);  // delay 0.5s after setup
}

void SanextMonoCU::dump_config() {
  ESP_LOGCONFIG(TAG, "SANEXT Mono CU: Address 0x%llX", this->address_);
  if (this->cooling_energy_sensor_)
    LOG_SENSOR("  ", "Cooling Energy Sensor: ", this->cooling_energy_sensor_);
  if (this->heating_energy_sensor_)
    LOG_SENSOR("  ", "Heating Energy Sensor: ", this->heating_energy_sensor_);
  if (this->power_sensor_)
    LOG_SENSOR("  ", "Power Sensor: ", this->power_sensor_);
  if (this->flow_sensor_)
    LOG_SENSOR("  ", "Flow Sensor: ", this->flow_sensor_);
  if (this->volume_sensor_)
    LOG_SENSOR("  ", "Volume Sensor: ", this->volume_sensor_);
  if (this->water_supply_temperature_sensor_)
    LOG_SENSOR("  ", "Water Supply Temperature Sensor: ", this->water_supply_temperature_sensor_);
  if (this->backwater_temperature_sensor_)
    LOG_SENSOR("  ", "Backwater Temperature Sensor: ", this->backwater_temperature_sensor_);
  if (this->connectivity_error_sensor_)
    LOG_BINARY_SENSOR("  ", "Connectivity Error Sensor: ", this->connectivity_error_sensor_);
  if (this->battery_power_alarm_sensor_)
    LOG_BINARY_SENSOR("  ", "Battery Power Alarm Sensor: ", this->battery_power_alarm_sensor_);
  if (this->flow_alarm_sensor_)
    LOG_BINARY_SENSOR("  ", "Flow Alarm Sensor: ", this->flow_alarm_sensor_);
  if (this->ee_fault_sensor_)
    LOG_BINARY_SENSOR("  ", "EE Fault Sensor: ", this->ee_fault_sensor_);
  if (this->temperature_less_3_degree_sensor_)
    LOG_BINARY_SENSOR("  ", "Temperature less 3 degree Sensor: ", this->temperature_less_3_degree_sensor_);
  if (this->temperature_more_95_degree_sensor_)
    LOG_BINARY_SENSOR("  ", "Temperature more 95 degree Sensor: ", this->temperature_more_95_degree_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

void SanextMonoCU::update() {
  if (this->phase_ != 0) {
    ESP_LOGW(TAG, "Skip update() coz previous was not finished!");
  } else {
    ESP_LOGV(TAG, "Time to Update");
    read_meter();
  }
}

void SanextMonoCU::loop() {
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
    }
    auto &command = this->commands_queue_.front();
    if (command == nullptr || this->process_command(command.get())) {
      this->commands_queue_.pop();
      this->phase_ = 0;
    }

  } else if (this->running_ && this->commands_queue_.empty()) {
    this->running_ = false;
    if (this->connectivity_error_sensor_)
      this->connectivity_error_sensor_->publish_state(this->error_);
  }
}

bool SanextMonoCU::process_command(SanextCommand *command) {
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
      this->tx_bytes_sending_ = 0;
      // header 0xFE,0xFE, start, type
      this->tx_buffer_[this->tx_bytes_sending_++] = 0xFE;
      this->tx_buffer_[this->tx_bytes_sending_++] = 0xFE;
      this->tx_buffer_[this->tx_bytes_sending_++] = 0x68;
      this->tx_buffer_[this->tx_bytes_sending_++] = 0x20;
      // address 7 bytes
      for (uint8_t i = 0; i < 7; i++) {
        uint64_t addr = (this->address_ >> (8 * i)) & 0xFF;
        this->tx_buffer_[this->tx_bytes_sending_++] = (uint8_t) addr;
      }
      // control code, length, d0, d1, SER
      this->tx_buffer_[this->tx_bytes_sending_++] = command->code;
      this->tx_buffer_[this->tx_bytes_sending_++] = command->request_length;
      if (command->request_length > 1)
        this->tx_buffer_[this->tx_bytes_sending_++] = command->d0;
      if (command->request_length > 2)
        this->tx_buffer_[this->tx_bytes_sending_++] = command->d1;
      this->tx_buffer_[this->tx_bytes_sending_++] = this->serial_++;
      // check sum
      uint8_t csum = 0;
      for (uint8_t i = 2; i < this->tx_bytes_sending_; i++)
        csum += this->tx_buffer_[i];
      this->tx_buffer_[this->tx_bytes_sending_++] = csum;
      // end
      this->tx_buffer_[this->tx_bytes_sending_++] = 0x16;
    } break;

    case 2: {
      // sending data
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: sending %d bytes", command->code, this->phase_, this->tx_bytes_sending_);
      this->write_array(this->tx_buffer_, this->tx_bytes_sending_);
      this->flush();
      // will wait rx_bytes_needed bytes
      this->rx_bytes_needed_ = 13 + command->response_length + 2;
      this->rx_bytes_received_ = 0;
      this->wait_time_ = millis() + READ_TIMEOUT;  // timeout N milliseconds
    } break;

    case 3: {
    } break;

    // receiving packet
    case 4: {
      // read data from UART with chunks
      for (uint16_t i = 0; i < CHUNK_SIZE && this->rx_bytes_received_ < this->rx_bytes_needed_ && available(); i++) {
        uint8_t c = this->read();
        // package should start from 0xFE, 0xFE bytes
        if (this->rx_bytes_received_ == 0 && c != 0xFE) {
          ESP_LOGV(TAG, "Command 0x%02X, phase %d: skip unknown first byte 0x%02X", command->code, this->phase_, c);
          continue;
        } else if (this->rx_bytes_received_ == 1 && c != 0xFE) {
          ESP_LOGV(TAG, "Command 0x%02X, phase %d: skip unknown second byte 0x%02X", command->code, this->phase_, c);
          this->rx_bytes_received_ = 0;
          continue;
        } else if (this->rx_bytes_received_ == 2 && c == 0xFE) {
          ESP_LOGV(TAG, "Command 0x%02X, phase %d: skip third byte 0xFE", command->code, this->phase_);
          continue;
        }
        this->rx_buffer_[this->rx_bytes_received_++] = c;
      }
      // wait complete packet
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
      }
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: received %d bytes", command->code, this->phase_, this->rx_bytes_received_);
    } break;

    // validating packet
    case 5: {
      // check fixed header
      if (this->rx_buffer_[0] != 0xFE || this->rx_buffer_[1] != 0xFE) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d got wrong header 0x%02X, 0x%02X (instead of 0xFE, 0xFE)", command->code, this->phase_,
                 this->rx_buffer_[0], this->rx_buffer_[1]);
        return process_error(command);
      }
      // check start
      if (this->rx_buffer_[2] != 0x68) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d got wrong start 0x%02X (instead of 0x68)", command->code, this->phase_,
                 this->rx_buffer_[2]);
        return process_error(command);
      }
      // check type
      if (this->rx_buffer_[3] != this->tx_buffer_[3]) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d got wrong type 0x%02X (instead of 0x%02X)", command->code, this->phase_,
                 this->rx_buffer_[3], this->tx_buffer_[3]);
        return process_error(command);
      }
      // check data length
      if (this->rx_buffer_[12] != command->response_length) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d got wrong data length %d (instead of %d)", command->code, this->phase_,
                 this->rx_buffer_[12], command->response_length);
        return process_error(command);
      }
      // TODO check SER
      // check sum
      uint8_t csum = 0;
      for (uint8_t i = 2; i < this->rx_bytes_needed_ - 2; i++)
        csum += this->rx_buffer_[i];
      if (this->rx_buffer_[this->rx_bytes_needed_ - 2] != csum) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d got wrong check sum 0x%02X (instead of 0x%02X)", command->code, this->phase_,
                 this->rx_buffer_[this->rx_bytes_needed_ - 2], csum);
        return process_error(command);
      }
      // check end
      if (this->rx_buffer_[this->rx_bytes_needed_ - 1] != 0x16) {
        ESP_LOGW(TAG, "Command 0x%02X, phase %d got wrong end 0x%02X (instead of 0x16)", command->code, this->phase_,
                 this->rx_buffer_[this->rx_bytes_needed_ - 1]);
        return process_error(command);
      }
      // take address (7 bytes)
      uint64_t addr = 0UL;
      for (uint8_t i = 0; i < 7; i++) {
        addr += (uint64_t)(this->rx_buffer_[4 + i]) << (8 * i);
      }
      if (this->address_ == DEFAULT_ADDRESS) {
        this->address_ = addr;
        ESP_LOGI(TAG, "Receive address: 0x%llX", this->address_);
      } else if (this->address_ != addr) {
        ESP_LOGW(TAG, "Receive data from device with unkown address: 0x%llX", addr);
      }
      // everything OK; ready to process data
      ESP_LOGV(TAG, "Command 0x%02X, phase %d validation OK", command->code, this->phase_);
      this->process_phase_ = 0;
    } break;

    // response processing
    case 6: {
      if (command->code == SANEXT_ReadMeter) switch(++this->process_phase_) {
        case 1: {
          // ESP_LOGD(TAG, "Current cooling capacity: %.2f kWh (0x%02X)", (float)bcd32(&this->rx_buffer_[16]) * 0.01, this->rx_buffer_[20]);
          if (this->rx_buffer_[20] != 0x05) ESP_LOGW(TAG, "Cooling energy unknown unit: 0x%02X", this->rx_buffer_[20]);
          else if (this->cooling_energy_sensor_) this->cooling_energy_sensor_->publish_state((float)bcd32(&this->rx_buffer_[16]) * 0.01);
          return false;
        };
        case 2: {
          // ESP_LOGD(TAG, "Current calories: %.2f kWh (0x%02X)", (float)bcd32(&this->rx_buffer_[21]) * 0.01, this->rx_buffer_[25]);
          if (this->rx_buffer_[25] != 0x05) ESP_LOGW(TAG, "Heating energy unknown unit: 0x%02X", this->rx_buffer_[25]);
          else if (this->heating_energy_sensor_) this->heating_energy_sensor_->publish_state((float)bcd32(&this->rx_buffer_[21]) * 0.01);
          return false;
        };
        case 3: {
          // ESP_LOGD(TAG, "Power: %.2f kW (0x%02X)", (float)bcd32(&this->rx_buffer_[26]) * 0.01, this->rx_buffer_[30]);
          if (this->rx_buffer_[30] != 0x17) ESP_LOGW(TAG, "Power unknown unit: 0x%02X", this->rx_buffer_[30]);
          else if (this->power_sensor_) this->power_sensor_->publish_state((float)bcd32(&this->rx_buffer_[26]) * 0.01);
          return false;
        };
        case 4: {
          // ESP_LOGD(TAG, "Instantaneous flow rate: %.2f m3/h (0x%02X)", (float)bcd32(&this->rx_buffer_[31]) * 0.01, this->rx_buffer_[35]);
          if (this->rx_buffer_[35] != 0x35) ESP_LOGW(TAG, "Flow unknown unit: 0x%02X", this->rx_buffer_[35]);
          else if (this->flow_sensor_) this->flow_sensor_->publish_state((float)bcd32(&this->rx_buffer_[31]) * 0.01);
          return false;
        };
        case 5: {
          // ESP_LOGD(TAG, "Volume: %.2f m3 (0x%02X)", (float)bcd32(&this->rx_buffer_[36]) * 0.01, this->rx_buffer_[40]);
          if (this->rx_buffer_[40] != 0x2C) ESP_LOGW(TAG, "Volume unknown unit: 0x%02X", this->rx_buffer_[40]);
          else if (this->volume_sensor_) this->volume_sensor_->publish_state((float)bcd32(&this->rx_buffer_[36]) * 0.01);
          return false;
        };
        case 6: {
          if (this->water_supply_temperature_sensor_)
            this->water_supply_temperature_sensor_->publish_state((float)bcd24(&this->rx_buffer_[41]) * 0.01);
          return false;
        };
        case 7: {
          if (this->backwater_temperature_sensor_)
            this->backwater_temperature_sensor_->publish_state((float)bcd24(&this->rx_buffer_[44]) * 0.01);
          return false;
        };
        case 8: {
          ESP_LOGD(TAG, "Working time: %ld hours", bcd24(&this->rx_buffer_[47]));
          return false;
        };
        case 9: {
          ESP_LOGD(TAG, "Current time: %ld %ld", bcd32(&this->rx_buffer_[53]), bcd24(&this->rx_buffer_[50]));
          return false;
        };
        case 10: {
          if (this->battery_power_alarm_sensor_) this->battery_power_alarm_sensor_->publish_state((this->rx_buffer_[58] & 0x01) > 0);
          return false;
        };
        case 11: {
          if (this->flow_alarm_sensor_) this->flow_alarm_sensor_->publish_state((this->rx_buffer_[58] & 0x08) > 0);
          return false;
        };
        case 12: {
          if (this->ee_fault_sensor_) this->ee_fault_sensor_->publish_state((this->rx_buffer_[58] & 0x20) > 0);
          return false;
        };
        case 13: {
          if (this->temperature_less_3_degree_sensor_) this->temperature_less_3_degree_sensor_->publish_state((this->rx_buffer_[58] & 0x40) > 0);
          return false;
        };
        case 14: {
          if (this->temperature_more_95_degree_sensor_) this->temperature_more_95_degree_sensor_->publish_state((this->rx_buffer_[58] & 0x80) > 0);
          return false;
        };
      }
    } break;

    // final phase
    case 7:
      ESP_LOGV(TAG, "Command 0x%02X, phase %d: command completed", command->code, this->phase_);
      return true;
  }

  this->phase_++;
  return false;
}

bool SanextMonoCU::process_error(SanextCommand *command, uint8_t error_code) {
  // restart command if having retries
  if (++this->retry_count_ <= 3) {
    ESP_LOGD(TAG, "Error on command %02x. Doing retry %d...", command->code, this->retry_count_);
    this->phase_ = 0;
    delay(500);
    return false;
  }
  this->error_ = true;
  return true;
}

}  // namespace sanext_mono_cu
}  // namespace esphome
