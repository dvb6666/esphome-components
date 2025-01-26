#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include <memory>
#include <queue>

namespace esphome {
namespace fingerprint_zw {

#define TX_BUFFER_SIZE 64
#define RX_BUFFER_SIZE 192
#define PACKAGE_HEADER_LENGTH 9
#define DATA_BUFFER_SIZE 512
#define SERIAL_NUMBER_SIZE 8

enum ZwAuraLEDState : uint8_t {
  BREATHING = 0x01,
  FLASHING = 0x02,
  ALWAYS_ON = 0x03,
  ALWAYS_OFF = 0x04,
  GRADUAL_ON = 0x05,
  GRADUAL_OFF = 0x06,
};

enum ZwAuraLEDColor : uint8_t {
  OFF = 0x00,
  BLUE = 0x01,
  GREEN = 0x02,
  CYAN = 0x03,
  RED = 0x04,
  PURPLE = 0x05,
  YELLOW = 0x06,
  WHITE = 0x07,
};

class ZwCommand {
 public:
  ZwCommand(uint8_t code_, uint16_t length_, uint16_t response_length_, uint8_t p1_ = 0, uint8_t p2_ = 0,
            uint8_t p3_ = 0, uint8_t p4_ = 0, uint8_t p5_ = 0)
      : code(code_), length(length_), response_length(response_length_), p1(p1_), p2(p2_), p3(p3_), p4(p4_), p5(p5_) {}
  uint8_t code;
  uint16_t length, response_length;
  uint8_t p1, p2, p3, p4, p5;
  uint32_t delay{0};
  bool packaged{false};
  void set_delay(uint32_t delay) { this->delay = delay; }
  void set_packaged() { this->packaged = true; }
};

typedef struct {
  uint16_t enroll_times, temp_size, database_size, score_level;
  uint32_t device_address;
  uint16_t pkt_size, baud_rate;
} ReadSysParaResponse;

typedef struct {
  uint16_t enroll_times, temp_size, database_size, score_level;
  uint32_t device_address;
  uint16_t pkt_size, baud_rate, prevent_false_fingerprints, fp_sensor;
  uint16_t encryption_level, enroll_logic, image_format, serial_port_delay;
  char product_sn[8], sw_version[8], manufacturer[8], sensor_name[8];
} ReadInfPageResponse;

#define PS_GetImage 0x01
#define PS_GenChar 0x02
#define PS_Search 0x04
#define PS_ValidTempleteNum 0x1D
#define PS_ReadSysPara 0x0F
#define PS_ReadlNFpage 0x16
#define PS_GetChipSN 0x34
#define PS_ControlBLN 0x3C
#define PS_DeletChar 0x0C
#define PS_Empty 0x0D
// #define PS_Getlmagelnfo 0x3D
// #define PS_CheckSensor 0x36

class ZwCommandGetImage : public ZwCommand {
 public:
  ZwCommandGetImage() : ZwCommand(PS_GetImage, 3, 3) {}
};
class ZwCommandGenChar : public ZwCommand {
 public:
  ZwCommandGenChar(uint8_t buffer = 1) : ZwCommand(PS_GenChar, 4, 3, buffer) {}
};
class ZwCommandSearch : public ZwCommand {
 public:
  ZwCommandSearch(uint16_t capacity, uint8_t buffer = 1)
      : ZwCommand(PS_Search, 8, 7, buffer, 0, 0, (uint8_t) (capacity >> 8), (uint8_t) (capacity & 0xFF)) {}
};

class ZwCommandReadSysPara : public ZwCommand {
 public:
  ZwCommandReadSysPara() : ZwCommand(PS_ReadSysPara, 3, 19) {}
};
class ZwCommandInfPage : public ZwCommand {
 public:
  ZwCommandInfPage() : ZwCommand(PS_ReadlNFpage, 3, 3) { set_packaged(); }
};
class ZwCommandValidTempleteNum : public ZwCommand {
 public:
  ZwCommandValidTempleteNum() : ZwCommand(PS_ValidTempleteNum, 3, 5) {}
};
class ZwCommandControlBLN : public ZwCommand {
 public:
  ZwCommandControlBLN(ZwAuraLEDState mode, ZwAuraLEDColor start, ZwAuraLEDColor end, uint8_t cycles)
      : ZwCommand(PS_ControlBLN, 7, 3, mode, start, end, cycles) {}
};
class ZwCommandGetChipSN : public ZwCommand {
 public:
  ZwCommandGetChipSN() : ZwCommand(PS_GetChipSN, 4, 35) {}
};
class ZwCommandDeleteOne : public ZwCommand {
 public:
  ZwCommandDeleteOne(uint16_t finger_id, uint16_t number)
      : ZwCommand(PS_DeletChar, 7, 3, (finger_id >> 8) & 0xFF, finger_id & 0xFF, (number >> 8) & 0xFF, number & 0xFF) {}
};
class ZwCommandDeleteAll : public ZwCommand {
 public:
  ZwCommandDeleteAll() : ZwCommand(PS_Empty, 3, 3) {}
};

class ZwComponent : public Component, public uart::UARTDevice {
 public:
  ZwComponent(uart::UARTComponent *uart) : uart::UARTDevice(uart) {
    memset(this->module_id_, 0, sizeof(this->module_id_));
  }

  static uint16_t htons(uint16_t a) { return ((a >> 8) & 0xff) | ((a & 0xff) << 8); };
  static uint32_t htonl(uint32_t a) {
    return ((a & 0xff000000) >> 24) | ((a & 0xff0000) >> 8) | ((a & 0xff00) << 8) | ((a & 0xff) << 24);
  };

  void start_scan();
  void start_register(uint16_t finger_id = 0, uint8_t role = 3, uint32_t delay = 2000);
  void aura_led_control(ZwAuraLEDState state, ZwAuraLEDColor color, uint8_t count = 0);
  void delete_one(uint16_t finger_id, uint16_t number = 1);
  void delete_all();
  void get_module_parameters();
  void get_flash_parameters();
  void get_serial_number();
  void get_fingerprints_count();

  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_sensor_power_pin(InternalGPIOPin *pin) { this->sensor_power_pin_ = pin; }
  void set_sensing_pin(InternalGPIOPin *pin) { this->sensing_pin_ = pin; }
  void set_idle_period_to_sleep(uint32_t period_ms) { this->idle_period_to_sleep_ms_ = period_ms; }
  void set_auto_led_off(bool led_off) { this->auto_led_off_ = led_off; }
  void set_error_sensor(binary_sensor::BinarySensor *sensor) { this->error_sensor_ = sensor; }
  void set_fingerprint_count_sensor(sensor::Sensor *sensor) { this->fingerprint_count_sensor_ = sensor; }
  void set_last_finger_id_sensor(sensor::Sensor *sensor) { this->last_finger_id_sensor_ = sensor; }
  void set_capacity_sensor(sensor::Sensor *sensor) { this->capacity_sensor_ = sensor; }

  void add_on_finger_scan_start_callback(std::function<void()> callback) {
    this->finger_scan_start_callback_.add(std::move(callback));
  }
  void add_on_finger_scan_matched_callback(std::function<void(uint16_t, uint16_t)> callback) {
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
  void add_on_Register_start_callback(std::function<void(uint8_t)> callback) {
    this->register_start_callback_.add(std::move(callback));
  }
  void add_on_Register_done_callback(std::function<void(uint8_t, uint16_t)> callback) {
    this->register_done_callback_.add(std::move(callback));
  }
  void add_on_Register_failed_callback(std::function<void(uint8_t)> callback) {
    this->register_failed_callback_.add(std::move(callback));
  }

 protected:
  void delay(uint32_t delay_ms) { this->sleep_time_ = millis() + delay_ms; }
  bool process_command(ZwCommand *command);
  bool process_error(ZwCommand *command, uint8_t error_code = 0x01);

 private:
  InternalGPIOPin *sensor_power_pin_{nullptr}, *sensing_pin_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  sensor::Sensor *fingerprint_count_sensor_{nullptr}, *last_finger_id_sensor_{nullptr}, *capacity_sensor_{nullptr};
  uint32_t idle_period_to_sleep_ms_{0};
  bool auto_led_off_{false}, need_led_off_{false};
  std::queue<std::unique_ptr<ZwCommand>> commands_queue_;
  bool last_touch_state_{false}, running_{false}, error_{false};
  char module_id_[2 * SERIAL_NUMBER_SIZE + 1] = {};
  char product_sn_[9] = {}, sw_version_[9] = {}, manufacturer_[9] = {}, sensor_name_[9] = {};
  uint32_t address_{0xFFFFFFFF};
  uint16_t capacity_{80};
  bool wait_package_{false};

  uint16_t phase_{0}, retry_count_{0};
  unsigned long sleep_time_{0}, wait_time_{0}, log_time_{0};
  uint16_t rx_bytes_needed_{0}, rx_bytes_received_{0}, data_bytes_received_{0};
  uint8_t tx_buffer_[TX_BUFFER_SIZE], rx_buffer_[RX_BUFFER_SIZE], data_buffer_[DATA_BUFFER_SIZE];

  CallbackManager<void()> finger_scan_start_callback_;
  CallbackManager<void(uint16_t, uint16_t)> finger_scan_matched_callback_;
  CallbackManager<void()> finger_scan_unmatched_callback_;
  CallbackManager<void()> finger_scan_misplaced_callback_;
  CallbackManager<void(uint8_t)> finger_scan_failed_callback_;
  CallbackManager<void(uint8_t)> register_start_callback_;
  CallbackManager<void(uint8_t, uint16_t)> register_done_callback_;
  CallbackManager<void(uint8_t)> register_failed_callback_;
};

class ZwScanStartTrigger : public Trigger<> {
 public:
  explicit ZwScanStartTrigger(ZwComponent *parent) {
    parent->add_on_finger_scan_start_callback([this]() { this->trigger(); });
  }
};

class ZwScanMatchedTrigger : public Trigger<uint16_t, uint16_t> {
 public:
  explicit ZwScanMatchedTrigger(ZwComponent *parent) {
    parent->add_on_finger_scan_matched_callback(
        [this](uint16_t finger_id, uint16_t score) { this->trigger(finger_id, score); });
  }
};

class ZwScanUnmatchedTrigger : public Trigger<> {
 public:
  explicit ZwScanUnmatchedTrigger(ZwComponent *parent) {
    parent->add_on_finger_scan_unmatched_callback([this]() { this->trigger(); });
  }
};

class ZwScanMisplacedTrigger : public Trigger<> {
 public:
  explicit ZwScanMisplacedTrigger(ZwComponent *parent) {
    parent->add_on_finger_scan_misplaced_callback([this]() { this->trigger(); });
  }
};

class ZwScanFailedTrigger : public Trigger<uint8_t> {
 public:
  explicit ZwScanFailedTrigger(ZwComponent *parent) {
    parent->add_on_finger_scan_failed_callback([this](uint8_t error_code) { this->trigger(error_code); });
  }
};

class ZwRegisterStartTrigger : public Trigger<uint8_t> {
 public:
  explicit ZwRegisterStartTrigger(ZwComponent *parent) {
    parent->add_on_Register_start_callback([this](uint8_t step) { this->trigger(step); });
  }
};

class ZwRegisterDoneTrigger : public Trigger<uint8_t, uint16_t> {
 public:
  explicit ZwRegisterDoneTrigger(ZwComponent *parent) {
    parent->add_on_Register_done_callback([this](uint8_t step, uint16_t finger_id) { this->trigger(step, finger_id); });
  }
};

class ZwRegisterFailedTrigger : public Trigger<uint8_t> {
 public:
  explicit ZwRegisterFailedTrigger(ZwComponent *parent) {
    parent->add_on_Register_failed_callback([this](uint8_t error_code) { this->trigger(error_code); });
  }
};

template<typename... Ts> class ZwStartScanAction : public Action<Ts...>, public Parented<ZwComponent> {
 public:
  void play(Ts... x) override { this->parent_->start_scan(); }
};

template<typename... Ts> class ZwStartRegisterAction : public Action<Ts...>, public Parented<ZwComponent> {
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

template<typename... Ts> class ZwAuraLEDControlAction : public Action<Ts...>, public Parented<ZwComponent> {
 public:
  TEMPLATABLE_VALUE(ZwAuraLEDState, state);
  TEMPLATABLE_VALUE(ZwAuraLEDColor, color);
  TEMPLATABLE_VALUE(uint8_t, count);
  void play(Ts... x) override {
    auto state = this->state_.value(x...);
    auto color = this->color_.value(x...);
    auto count = this->count_.value(x...);
    this->parent_->aura_led_control(state, color, count);
  }
};

template<typename... Ts> class ZwDeleteAction : public Action<Ts...>, public Parented<ZwComponent> {
 public:
  TEMPLATABLE_VALUE(uint16_t, finger_id);
  void play(Ts... x) override {
    auto finger_id = this->finger_id_.value(x...);
    this->parent_->delete_one(finger_id);
  }
};

template<typename... Ts> class ZwDeleteAllAction : public Action<Ts...>, public Parented<ZwComponent> {
 public:
  void play(Ts... x) override { this->parent_->delete_all(); }
};

}  // namespace fingerprint_zw
}  // namespace esphome
