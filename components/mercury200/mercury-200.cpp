#include "mercury-200.h"

namespace esphome {
namespace mercury200 {

static const char *const TAG = "mercury200";

Mercury200::Mercury200(uart::UARTComponent *uart, uint32_t address) : uart::UARTDevice(uart), address_(address) {
  for (uint8_t i = 0; i < MAX_TARIFF_COUNT; i++)
    this->sensor_energy_[i] = nullptr;
}

void Mercury200::setup() {
  this->phase_ = 0;
  if (this->dir_pin_) {
    this->dir_pin_->setup();
    this->dir_pin_->digital_write(false);
  }
  // read old unknown/unused data
  while (this->available())
    this->read();

  // commands
  if (this->all_commands_) {
    this->commands_.push_back(new GetSerialNumberCommand([this](uint32_t addr) { ESP_LOGD(TAG, "Serial number: %d", addr); }));
    this->commands_.push_back(new GetVersionCommand(
        [this](uint16_t ver, uint32_t data_ver) { ESP_LOGD(TAG, "Version: %d.%d (%06x)", ver & 0xff, (ver >> 8) & 0xff, data_ver); }));
    this->commands_.push_back(new GetDateFabricCommand(
        [this](uint8_t day, uint8_t month, uint16_t year) { ESP_LOGD(TAG, "DateFabric: %d.%d.%d", day, month, year); }));
    this->commands_.push_back(new GetTimeCommand([this](uint32_t tm) { ESP_LOGD(TAG, "Time: %d", tm); }));
    this->commands_.push_back(new GetLastTurnOffCommand([this](uint32_t tm) { ESP_LOGD(TAG, "LastTurnOff: %d", tm); }));
    this->commands_.push_back(new GetLastTurnOnCommand([this](uint32_t tm) { ESP_LOGD(TAG, "LastTurnOn: %d", tm); }));
    this->commands_.push_back(new GetTarifsCountCommand([this](uint8_t count) { ESP_LOGD(TAG, "TarifsCount: %d", count); }));
  }
  if (this->all_commands_ || this->sensor_battery_)
    this->commands_.push_back(new GetBatteryCommand([this](float voltage) {
      ESP_LOGD(TAG, "Battery: %.2f", voltage);
      if (this->sensor_battery_)
        this->sensor_battery_->publish_state(voltage);
    }));
  if (this->all_commands_ || this->sensor_voltage_ || this->sensor_current_ || this->sensor_power_)
    this->commands_.push_back(new GetUIPCommand([this](float v, float i, uint32_t p) {
      ESP_LOGD(TAG, "U.I.P.: %.1f, %.2f, %d", v, i, p);
      if (this->sensor_voltage_)
        this->sensor_voltage_->publish_state(v);
      if (this->sensor_current_)
        this->sensor_current_->publish_state(i);
      if (this->sensor_power_)
        this->sensor_power_->publish_state(p);
    }));
  this->commands_.push_back(new GetCountersCommand([this](float t1, float t2, float t3, float t4) {
    ESP_LOGD(TAG, "Counters: %.2f, %.2f, %.2f, %.2f", t1, t2, t3, t4);
    float *arr[MAX_TARIFF_COUNT] = {&t1, &t2, &t3, &t4};
    for (uint8_t i = 0; i < MAX_TARIFF_COUNT; i++)
      if (this->sensor_energy_[i])
        this->sensor_energy_[i]->publish_state(*(arr[i]));
  }));
  // startup delay
  delay(this->startup_delay_);
}

void Mercury200::dump_config() {
  ESP_LOGCONFIG(TAG, "Mercury 200.02 '%d'", this->address_);
  ESP_LOGCONFIG(TAG, "  All Commands: %s", this->all_commands_ ? "yes" : "no");
  if (this->dir_pin_) {
    LOG_PIN("  Direction Pin: ", this->dir_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Direction Pin: no");
  }
  for (uint8_t i = 0; i < MAX_TARIFF_COUNT; i++)
    if (this->sensor_energy_[i])
      LOG_SENSOR("  ", "Energy Sensor", this->sensor_energy_[i]);
  if (this->sensor_voltage_)
    LOG_SENSOR("  ", "Voltage Sensor", this->sensor_voltage_);
  if (this->sensor_current_)
    LOG_SENSOR("  ", "Current Sensor", this->sensor_current_);
  if (this->sensor_power_)
    LOG_SENSOR("  ", "Power Sensor", this->sensor_power_);
  if (this->sensor_battery_)
    LOG_SENSOR("  ", "Battery Sensor", this->sensor_battery_);
  if (this->sensor_error_)
    LOG_BINARY_SENSOR("  ", "Error Sensor", this->sensor_error_);
  ESP_LOGCONFIG(TAG, "  Startup Delay: %d", this->startup_delay_);
  LOG_UPDATE_INTERVAL(this);
}

void Mercury200::loop() {
  if (this->phase_ == 0 || this->sleep_time_ > millis())
    return;

  uint32_t phase = this->phase_ % 8;
  uint32_t cmd_idx = this->phase_ / 8;
  if (cmd_idx >= this->commands_.size() || this->error_) {
    this->phase_ = 0;
    if (this->sensor_error_)
      this->sensor_error_->publish_state(this->error_);
    return;
  }

  switch (phase) {

  case 1: { // preparing command data
    *((uint32_t *)&this->tx_buffer_[0]) = Command::htonl(this->address_ % 1000000);
    this->tx_buffer_[4] = this->commands_[cmd_idx]->code();
    uint16_t crc = crc16(this->tx_buffer_, 5);
    this->tx_buffer_[5] = crc & 0xff;
    this->tx_buffer_[6] = (crc >> 8) & 0xff;
    this->rx_bytes_needed_ = 7 + this->commands_[cmd_idx]->data_size();
    this->rx_bytes_received_ = 0;
    this->wait_time_ = millis() + 1000; // timeout 1000 ms
  } break;

  case 2: { // set dir_pin to state SEND
    if (this->dir_pin_) {
      ESP_LOGV(TAG, "Set dir_pin to state SEND");
      this->dir_pin_->digital_write(true);
      delay(5);
    }
  } break;

  case 3: { // sending command data
    ESP_LOGV(TAG, "Sending command 0x%02x", this->commands_[cmd_idx]->code());
    this->write_array(this->tx_buffer_, 7);
    this->flush();
    delay(5);
  } break;

  case 4: { // delay after command sent
    if (this->dir_pin_) {
      this->dir_pin_->digital_write(false);
      ESP_LOGV(TAG, "Set dir_pin to state RECV");
    }
  } break;

  case 5: { // receiving data
    while (this->available() && this->rx_bytes_received_ < this->rx_bytes_needed_)
      this->rx_buffer_[this->rx_bytes_received_++] = this->read();
    if (this->rx_bytes_received_ < this->rx_bytes_needed_) {
      if (this->wait_time_ < millis()) {
        ESP_LOGD(TAG, "Timed out command 0x%02x", this->commands_[cmd_idx]->code());
        this->error_ = true;
        this->phase_ += 2; // skip two next phases (validating and processing)
      } else {
        ESP_LOGV(TAG, "Waiting next %d bytes", this->rx_bytes_needed_ - this->rx_bytes_received_);
        return;
      }
    }
  } break;

  case 6: { // validating data
    uint16_t recv_crc = ((uint16_t)this->rx_buffer_[this->rx_bytes_needed_ - 1] << 8) | this->rx_buffer_[this->rx_bytes_needed_ - 2];
    uint16_t calc_crc = crc16(this->rx_buffer_, this->rx_bytes_needed_ - 2);
    if (recv_crc != calc_crc) {
      ESP_LOGD(TAG, "Bad checksum (0x%04x instead of 0x%04x)", recv_crc, calc_crc);
      this->error_ = true;
      this->phase_++;
    }
  } break;

  case 7: { // processing command
    ESP_LOGV(TAG, "Processing command 0x%02x", this->rx_buffer_[4]);
    this->commands_[cmd_idx]->process(this->rx_buffer_ + 5);
  } break;

  } // end case

  this->phase_++;
}

void Mercury200::update() {
  if (this->phase_ != 0) {
    ESP_LOGW(TAG, "Skip update() coz previous was not finished!");
  } else {
    ESP_LOGV(TAG, "Time to Update");
    this->error_ = false; // reset error
    this->phase_ = 1;
  }
}

/*
https://github.com/RocketFox2409/MercuryESPHome/blob/main/mercury/mercury-200.02.h
*/
uint16_t Mercury200::crc16(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

uint16_t Command::bcd16(const uint8_t *data, uint8_t len) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (i > 0)
      sum *= 100;
    sum += bcd(data[i]);
  }
  return sum;
}

uint32_t Command::bcd32(const uint8_t *data, uint8_t len) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (i > 0)
      sum *= 100;
    sum += bcd(data[i]);
  }
  return sum;
}

time_t Command::timestamp(const uint8_t *data) {
  tm tm{};
  tm.tm_hour = bcd(data[1]);
  tm.tm_min = bcd(data[2]);
  tm.tm_sec = bcd(data[3]);
  tm.tm_mday = bcd(data[4]);
  tm.tm_mon = bcd(data[5]) - 1;
  tm.tm_year = 2000 + bcd(data[6]) - 1900;
  tm.tm_isdst = 0;
  return mktime(&tm);
}

} // namespace mercury200
} // namespace esphome
