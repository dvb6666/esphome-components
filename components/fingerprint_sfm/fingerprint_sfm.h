#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include <memory>
#include <queue>

namespace esphome {
namespace fingerprint_sfm {

#define COMMAND_SIZE 8
#define MODULE_ID_SIZE 8
#define PACKAGE_MODULE_ID_SIZE MODULE_ID_SIZE + 3
#define RX_BUFFER_SIZE PACKAGE_MODULE_ID_SIZE

#define ACK_SUCCESS 0x00       // Выполнение успешное
#define ACK_FAIL 0x01          // Ошибка выполнения
#define ACK_FULL 0x04          // База данных заполнена
#define ACK_NOUSER 0x05        // Такого пользователя нет
#define ACK_USER_EXIST 0x07    // Пользователь уже существует
#define ACK_TIMEOUT 0x08       // Тайм-аут получения изображения
#define ACK_HARDWAREERROR 0x0A // Аппаратная ошибка
#define ACK_IMAGEERROR 0x10    // Ошибка изображения
#define ACK_BREAK 0x18         // Завершить текущую инструкцию
#define ACK_ALGORITHMFAIL 0x11 // Обнаружение пленочной атаки
#define ACK_HOMOLOGYFAIL 0x12  // Ошибка проверки гомологии

enum SfmColor : uint8_t {
  YELLOW = 0x01,
  PURPLE = 0x02,
  RED = 0x03,
  CYAN = 0x04,
  GREEN = 0x05,
  BLUE = 0x06,
  OFF = 0x07,
};

class SfmCommand {
public:
  SfmCommand(uint8_t code_, uint8_t p1_ = 0, uint8_t p2_ = 0, uint8_t p3_ = 0, uint32_t delay_ = 0)
      : code(code_), p1(p1_), p2(p2_), p3(p3_), delay(delay_) {}
  uint8_t code, p1, p2, p3;
  uint32_t delay;
};

class SfmComponent : public Component, public uart::UARTDevice {
public:
  SfmComponent(uart::UARTComponent *uart, InternalGPIOPin *sensor_power_pin)
      : uart::UARTDevice(uart), sensor_power_pin_(sensor_power_pin) {};

  void start_scan();
  void start_register(uint16_t finger_id = 0, uint8_t role = 3, uint32_t delay = 2000);
  void set_color(SfmColor start, SfmColor end = SfmColor::OFF, uint16_t period = 500, uint32_t delay = 0);
  void delete_one(uint16_t finger_id);
  void delete_all();
  void get_fingerprints_count();
  void get_module_number();

  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_sensing_pin(InternalGPIOPin *pin) { this->sensing_pin_ = pin; }
  void set_idle_period_to_sleep_ms(uint32_t period_ms) { this->idle_period_to_sleep_ms_ = period_ms; }
  void set_dir_pin(InternalGPIOPin *pin) { this->dir_pin_ = pin; }
  void set_error_sensor(binary_sensor::BinarySensor *sensor) { this->error_sensor_ = sensor; }
  void set_fingerprint_count_sensor(sensor::Sensor *sensor) { this->fingerprint_count_sensor_ = sensor; }
  void set_last_finger_id_sensor(sensor::Sensor *last_finger_id_sensor) { this->last_finger_id_sensor_ = last_finger_id_sensor; }

  void add_on_finger_scan_start_callback(std::function<void()> callback) { this->finger_scan_start_callback_.add(std::move(callback)); }
  void add_on_finger_scan_matched_callback(std::function<void(uint16_t, uint8_t)> callback) {
    this->finger_scan_matched_callback_.add(std::move(callback));
  }
  void add_on_finger_scan_unmatched_callback(std::function<void()> callback) {
    this->finger_scan_unmatched_callback_.add(std::move(callback));
  }
  void add_on_finger_scan_misplaced_callback(std::function<void()> callback) {
    this->finger_scan_misplaced_callback_.add(std::move(callback));
  }
  void add_on_finger_scan_failed_callback(std::function<void(uint8_t)> callback) {
    this->finger_scan_failed_callback_.add(std::move(callback));
  }
  void add_on_Register_start_callback(std::function<void(uint8_t)> callback) { this->register_start_callback_.add(std::move(callback)); }
  void add_on_Register_done_callback(std::function<void(uint8_t, uint16_t)> callback) {
    this->register_done_callback_.add(std::move(callback));
  }
  void add_on_Register_failed_callback(std::function<void(uint8_t)> callback) { this->register_failed_callback_.add(std::move(callback)); }

protected:
  void delay(uint32_t delay_ms) { this->sleep_time_ = millis() + delay_ms; }
  bool process_command(SfmCommand *command);
  bool process_error(SfmCommand *command, uint8_t error_code = ACK_FAIL);

private:
  InternalGPIOPin *sensor_power_pin_, *sensing_pin_{nullptr}, *dir_pin_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  sensor::Sensor *fingerprint_count_sensor_{nullptr};
  sensor::Sensor *last_finger_id_sensor_{nullptr};
  uint32_t idle_period_to_sleep_ms_{0};
  std::queue<std::unique_ptr<SfmCommand>> commands_queue_;
  bool last_touch_state_{false}, running_{false}, error_{false}, package_wainting_;
  std::string module_id_{16, '0'};

  uint16_t phase_{0};
  unsigned long sleep_time_{0}, wait_time_{0}, log_time_{0};
  uint16_t rx_bytes_needed_{0}, rx_bytes_received_{0};
  uint8_t tx_buffer_[COMMAND_SIZE], rx_buffer_[RX_BUFFER_SIZE];

  CallbackManager<void()> finger_scan_start_callback_;
  CallbackManager<void(uint16_t, uint8_t)> finger_scan_matched_callback_;
  CallbackManager<void()> finger_scan_unmatched_callback_;
  CallbackManager<void()> finger_scan_misplaced_callback_;
  CallbackManager<void(uint8_t)> finger_scan_failed_callback_;
  CallbackManager<void(uint8_t)> register_start_callback_;
  CallbackManager<void(uint8_t, uint16_t)> register_done_callback_;
  CallbackManager<void(uint8_t)> register_failed_callback_;
};

class SfmScanStartTrigger : public Trigger<> {
public:
  explicit SfmScanStartTrigger(SfmComponent *parent) {
    parent->add_on_finger_scan_start_callback([this]() { this->trigger(); });
  }
};

class SfmScanMatchedTrigger : public Trigger<uint16_t, uint8_t> {
public:
  explicit SfmScanMatchedTrigger(SfmComponent *parent) {
    parent->add_on_finger_scan_matched_callback([this](uint16_t finger_id, uint8_t role) { this->trigger(finger_id, role); });
  }
};

class SfmScanUnmatchedTrigger : public Trigger<> {
public:
  explicit SfmScanUnmatchedTrigger(SfmComponent *parent) {
    parent->add_on_finger_scan_unmatched_callback([this]() { this->trigger(); });
  }
};

class SfmScanMisplacedTrigger : public Trigger<> {
public:
  explicit SfmScanMisplacedTrigger(SfmComponent *parent) {
    parent->add_on_finger_scan_misplaced_callback([this]() { this->trigger(); });
  }
};

class SfmScanFailedTrigger : public Trigger<uint8_t> {
public:
  explicit SfmScanFailedTrigger(SfmComponent *parent) {
    parent->add_on_finger_scan_failed_callback([this](uint8_t error_code) { this->trigger(error_code); });
  }
};

class SfmRegisterStartTrigger : public Trigger<uint8_t> {
public:
  explicit SfmRegisterStartTrigger(SfmComponent *parent) {
    parent->add_on_Register_start_callback([this](uint8_t step) { this->trigger(step); });
  }
};

class SfmRegisterDoneTrigger : public Trigger<uint8_t, uint16_t> {
public:
  explicit SfmRegisterDoneTrigger(SfmComponent *parent) {
    parent->add_on_Register_done_callback([this](uint8_t step, uint16_t finger_id) { this->trigger(step, finger_id); });
  }
};

class SfmRegisterFailedTrigger : public Trigger<uint8_t> {
public:
  explicit SfmRegisterFailedTrigger(SfmComponent *parent) {
    parent->add_on_Register_failed_callback([this](uint8_t error_code) { this->trigger(error_code); });
  }
};

template <typename... Ts> class SfmStartScanAction : public Action<Ts...>, public Parented<SfmComponent> {
public:
  void play(Ts... x) override { this->parent_->start_scan(); }
};

template <typename... Ts> class SfmStartRegisterAction : public Action<Ts...>, public Parented<SfmComponent> {
public:
  TEMPLATABLE_VALUE(uint16_t, finger_id);
  TEMPLATABLE_VALUE(uint8_t, role);
  TEMPLATABLE_VALUE(uint32_t, delay);
  void play(Ts... x) override {
    auto finger_id = this->finger_id_.value(x...);
    auto role = this->role_.value(x...);
    auto delay = this->delay_.value(x...);
    this->parent_->start_register(finger_id, role, delay);
  }
};

template <typename... Ts> class SfmSetColorAction : public Action<Ts...>, public Parented<SfmComponent> {
public:
  TEMPLATABLE_VALUE(SfmColor, start);
  TEMPLATABLE_VALUE(SfmColor, end);
  TEMPLATABLE_VALUE(uint16_t, period);
  TEMPLATABLE_VALUE(uint32_t, delay);
  void play(Ts... x) override {
    auto start = this->start_.value(x...);
    auto end = this->end_.value(x...);
    auto period = this->period_.value(x...);
    auto delay = this->delay_.value(x...);
    this->parent_->set_color(start, end, period, delay);
  }
};

template <typename... Ts> class SfmDeleteAction : public Action<Ts...>, public Parented<SfmComponent> {
public:
  TEMPLATABLE_VALUE(uint16_t, finger_id);
  void play(Ts... x) override {
    auto finger_id = this->finger_id_.value(x...);
    this->parent_->delete_one(finger_id);
  }
};

template <typename... Ts> class SfmDeleteAllAction : public Action<Ts...>, public Parented<SfmComponent> {
public:
  void play(Ts... x) override { this->parent_->delete_all(); }
};

// template <typename... Ts> class SfmDelayAction : public Action<Ts...>, public Parented<SfmComponent> {
// public:
//   TEMPLATABLE_VALUE(uint32_t, delay);
//   void play(Ts... x) override {
//     auto delay = this->delay_.value(x...);
//     this->parent_->delay(delay);
//   }
// };

} // namespace fingerprint_sfm
} // namespace esphome
