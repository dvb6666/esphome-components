#include "sfm_v1_7.h"

namespace esphome {
namespace sfm_v1_7 {

#define ACK_SUCCESS 0x00 //Выполнение успешное
#define ACK_FAIL 0x01 //Ошибка выполнения
#define ACK_FULL 0x04 //База данных заполнена
#define ACK_NOUSER 0x05 //Такого пользователя нет
#define ACK_USER_EXIST 0x07 //Пользователь уже существует
#define ACK_TIMEOUT 0x08 //Тайм-аут получения изображения
#define ACK_HARDWAREERROR 0x0A //Аппаратная ошибка
#define ACK_IMAGEERROR 0x10 //Ошибка изображения
#define ACK_BREAK 0x18 // Завершить текущую инструкцию
#define ACK_ALGORITHMFAIL 0x11 //Обнаружение пленочной атаки
#define ACK_HOMOLOGYFAIL 0x12 //Ошибка проверки гомологии

#define LED_TIME 50 // 500 ms
#define PHASES_COUNT 10
#define POWER_ON_DELAY 200
#define DIR_PIN_DELAY 5
#define LOG_WAIT_INTERVAL 1000
#define CHUNK_SIZE 34

#define skip_next_phases(er) { this->phase_ += (PHASES_COUNT - phase); this->error_ = er; return; }
#define return_to_phase(ph) { this->phase_ += (ph - phase); return; }

static const char *const TAG = "sfm_v1_7";

Command startLed = { 0xC3, 0x03, 0x07, LED_TIME, 200, false };
Command recognition1vN = { 0x0C, 0, 0, 0, 0, false };
Command stopLed = { 0xC3, 0x05, 0x05, LED_TIME, 200, true };
CommandBatch scanBatch { { &startLed, &recognition1vN, &stopLed } };

Command ledYellow = { 0xC3, 0x01, 0x07, LED_TIME, 0, false };
Command regFirst = { 0x01, 0x00, 0x00, 0x00, 2000, false };
Command ledPurple = { 0xC3, 0x02, 0x07, LED_TIME, 0, false };
Command regSecond = { 0x02, 0x00, 0x00, 0x00, 2000, false };
Command ledBlue = { 0xC3, 0x06, 0x07, LED_TIME, 0, false };
Command regThird = { 0x03, 0x00, 0x00, 0x00, 0, false };
Command ledGreen = { 0xC3, 0x05, 0x05, LED_TIME, 200, true };
CommandBatch registerBatch { { &ledYellow, &regFirst, &ledPurple, &regSecond, &ledBlue, &regThird, &ledGreen } };

Command setColor = { 0xC3, 0x05, 0x05, LED_TIME, 2000, false };
CommandBatch colorBatch { { &setColor } };

void SFM_v1_7::start_scan() {
  ESP_LOGD(TAG, "Start scan batch");
  this->batch_ = &scanBatch;
  this->phase_ = 1;
}

void SFM_v1_7::start_register(uint16_t uid, uint8_t role) {
  ESP_LOGD(TAG, "Start register batch: UID %d (0x%04X), Role %d", uid, uid, role);
  regFirst.p1 = (uid >> 8) & 0xFF;
  regFirst.p2 = uid & 0xFF;
  regFirst.p3 = role & 0x03;
  this->batch_ = &registerBatch;
  this->phase_ = 1;
}

void SFM_v1_7::set_color(SFM_Color start, SFM_Color end) {
  ESP_LOGD(TAG, "Set color (%d, %d)", start, end);
  setColor.p1 = start;
  setColor.p2 = end;
  this->batch_ = &colorBatch;
  this->phase_ = 1;
}

void SFM_v1_7::setup() {
  this->phase_ = 0;
  this->last_touch_state_ = false;

  this->vcc_pin_->setup();
  this->vcc_pin_->digital_write(this->vcc_always_on_);

  if (this->dir_pin_) {
    this->dir_pin_->setup();
    this->dir_pin_->digital_write(false);
  }
  if (this->irq_pin_) {
    this->irq_pin_->setup();
  }

  // read old unknown/unused data
  while (this->available())
    this->read();
}

void SFM_v1_7::dump_config() {
  ESP_LOGCONFIG(TAG, "SFM V1.7");
  LOG_PIN("  VCC Pin: ", this->vcc_pin_);
  ESP_LOGCONFIG(TAG, "  VCC Always On: %s", this->vcc_always_on_ ? "true" : "false");
  if (this->dir_pin_) {
    LOG_PIN("  Direction Pin: ", this->dir_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Direction Pin: disabled");
  }
  if (this->irq_pin_) {
    LOG_PIN("  Touch IRQ Pin: ", this->irq_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Touch Mode: disabled");
  }
  if (this->sensor_error_) {
    LOG_BINARY_SENSOR("  ", "Error Sensor", this->sensor_error_);
  }
}

void SFM_v1_7::loop() {
  if (this->sleep_time_ != 0 && this->sleep_time_ > millis())
    return;

  if (this->phase_ == 0) {
    if (this->irq_pin_ && !this->last_touch_state_ && this->irq_pin_->digital_read()) {
      ESP_LOGD(TAG, "Sensor touched!");
      this->last_touch_state_ = true;
      this->start_scan();
    } else if (this->irq_pin_ && this->last_touch_state_ && !this->irq_pin_->digital_read()) {
      ESP_LOGD(TAG, "Sensor released!");
      this->last_touch_state_ = false;
    }
    return;
  }

  if (this->batch_ == NULL) {
    ESP_LOGW(TAG, "Unknown command batch started!");
    this->phase_ = 0;
    return;
  }

  uint16_t cmd_idx = this->phase_ / PHASES_COUNT;
  uint16_t phase = this->phase_ % PHASES_COUNT;

  // first command starts
  if (cmd_idx == 0 && phase == 1) {
    this->error_ = false;
    if (!this->vcc_always_on_) { // power-on device
      ESP_LOGV(TAG, "Phase[%d][%d]: set vcc_pin to state 'power-on'", cmd_idx, phase);
      this->vcc_pin_->digital_write(true);
      delay(POWER_ON_DELAY);
    }
  }
  // all commands finished
  else if (cmd_idx >= this->batch_->commands.size()) {
    if (!this->vcc_always_on_) { // power-off device
      ESP_LOGV(TAG, "Phase[%d][%d]: set vcc_pin to state 'power-off'", cmd_idx, phase);
      this->vcc_pin_->digital_write(false);
      // delay(POWER_ON_DELAY);
    }
    if (this->sensor_error_)
      this->sensor_error_->publish_state(this->error_);
    ESP_LOGV(TAG, "All phases done");
    this->phase_ = 0;
    return;
  }

  switch (phase) {

  case 1: {
    // skip not "always" commands when error happened on previous steps
    if (this->error_ && !this->batch_->commands[cmd_idx]->run_always) {
      ESP_LOGV(TAG, "Phase[%d][%d]: skip command after error", cmd_idx, phase);
      skip_next_phases(true)
    }
  } break;

  case 2: {
    // read old unknown/unused data
    uint16_t unused;
    for (unused = 0; unused < CHUNK_SIZE && this->available(); unused++)
      this->read();
    if (unused > 0) {
      ESP_LOGV(TAG, "Phase[%d][%d]: received %d unknown/unused bytes", cmd_idx, phase, unused);
      return;
    }
    // prepare data to send
    this->tx_buffer_[0] = this->tx_buffer_[7] = 0xF5;
    this->tx_buffer_[1] = this->batch_->commands[cmd_idx]->code;
    this->tx_buffer_[2] = this->batch_->commands[cmd_idx]->p1;
    this->tx_buffer_[3] = this->batch_->commands[cmd_idx]->p2;
    this->tx_buffer_[4] = this->batch_->commands[cmd_idx]->p3;
    this->tx_buffer_[5] = this->tx_buffer_[6] = 0x00;
    for(uint8_t i = 1; i <= 5; i++) this->tx_buffer_[6] ^= this->tx_buffer_[i];
  } break;

  case 3: {
    // before sending command
    switch(this->tx_buffer_[1]) {
      case 0x01:
      case 0x02:
      case 0x03: {
        ESP_LOGD(TAG, "Please put your finger");
      } break;
    }
    // set dir_pin to state SEND before sending command
    if (this->dir_pin_) {
      ESP_LOGV(TAG, "Phase[%d][%d]: set dir_pin to state SEND", cmd_idx, phase);
      this->dir_pin_->digital_write(true);
      delay(DIR_PIN_DELAY);
    }
  } break;

  case 4: {
    // sending command data
    ESP_LOGV(TAG, "Phase[%d][%d]: sending %d bytes", cmd_idx, phase, COMMAND_SIZE);
    this->write_array(this->tx_buffer_, COMMAND_SIZE);
    this->flush();
    this->rx_bytes_needed_ = COMMAND_SIZE;
    this->rx_bytes_received_ = 0;
    this->wait_time_ = millis() + 8100; // timeout N milliseconds
  } break;

  case 5: {
    // set dir_pin to state RECV after command sent
    if (this->dir_pin_) {
      ESP_LOGV(TAG, "Phase[%d][%d]: set dir_pin to state RECV", cmd_idx, phase);
      this->dir_pin_->digital_write(false);
      delay(DIR_PIN_DELAY);
    }
  } break;

  case 6: {
    // receiving packet
    for (uint16_t i = 0; i < CHUNK_SIZE && this->rx_bytes_received_ < this->rx_bytes_needed_ && this->available(); i++) {
      uint8_t c = this->read();
      if (this->rx_bytes_received_ == 0 && c != 0xF5) {
        ESP_LOGV(TAG, "Phase[%d][%d]: skip unknown first byte %02x", cmd_idx, phase, c);
        continue;
      }
      this->rx_buffer_[this->rx_bytes_received_++] = c;
    }
    if (this->rx_bytes_received_ < this->rx_bytes_needed_) {
      unsigned long current_time = millis();
      if (this->wait_time_ < current_time) {
        ESP_LOGD(TAG, "Phase[%d][%d]: timed out", cmd_idx, phase);
        skip_next_phases(true);
      } else {
        if (this->log_time_ < current_time) {
          ESP_LOGV(TAG, "Phase[%d][%d]: waiting next %d bytes", cmd_idx, phase, this->rx_bytes_needed_ - this->rx_bytes_received_);
          this->log_time_ = current_time + LOG_WAIT_INTERVAL; // next log in LOG_WAIT_INTERVAL ms
        }
        return;
      }
    }
  } break;

  case 7: {
    // validating packet
    uint8_t csum = 0;
    for(uint8_t i = 1; i <= 5; i++) csum ^= this->rx_buffer_[i];
    if (csum != this->rx_buffer_[6]) {
      ESP_LOGW(TAG, "Phase[%d][%d]: wrong checksum (0x%02X instead of 0x%02X)", cmd_idx, phase, this->rx_buffer_[6], csum);
      skip_next_phases(true);
    } else if (this->tx_buffer_[1] != this->rx_buffer_[1]) {
      ESP_LOGW(TAG, "Phase[%d][%d]: got another command response (0x%02X instead of 0x%02X)", cmd_idx, phase, this->rx_buffer_[1], this->tx_buffer_[1]);
      skip_next_phases(true);
    }
    ESP_LOGV(TAG, "Phase[%d][%d]: validation Ok", cmd_idx, phase);
  } break;

  case 8: {
    // response processing
    uint8_t q1 = this->rx_buffer_[2];
    uint8_t q2 = this->rx_buffer_[3];
    uint8_t q3 = this->rx_buffer_[4];
    if (!(q3 == ACK_SUCCESS || (this->rx_buffer_[1] == 0x0C && q3 >= 1 && q3 <= 3))) {
      ESP_LOGW(TAG, "Phase[%d][%d]: got error code 0x%02X", cmd_idx, phase, q3);
      skip_next_phases(true);
    }
    switch(this->rx_buffer_[1]) {
      case 0x01:
      case 0x02: {
        ESP_LOGD(TAG, "Please releases your finger");
      } break;

      case 0x03: {
        uint16_t uid = (q1 << 8) | q2;
        ESP_LOGI(TAG, "Successfully register: UID %d (0x%04X)", uid, uid);
      } break;

      case 0x0C: {
        uint16_t uid = (q1 << 8) | q2;
        if (uid == 0) {
          ESP_LOGI(TAG, "Not found");
        } else {
          ESP_LOGI(TAG, "Successfully found: UID %d (0x%04X), Role %d", uid, uid, q3);
        }
      } break;
    }
  } break;

  case 9: {
    // delay after command executed
    if (this->batch_->commands[cmd_idx]->delay > 0) {
      ESP_LOGV(TAG, "Phase[%d][%d]: delay %d ms", cmd_idx, phase, this->batch_->commands[cmd_idx]->delay);
      delay(this->batch_->commands[cmd_idx]->delay);
    }
  } break;

  }
  this->phase_++;
}

} // namespace sfm_v1_7
} // namespace esphome
