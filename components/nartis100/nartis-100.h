#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "time.h"

namespace esphome {
namespace nartis100 {

#define MAX_TARIFF_COUNT 4

#define PKT_BUFF_MAX_LEN    128         /* max len read from uart   */
#define UART_BUFF_SIZE 512

#define CLIENT_ADDRESS  0x20
#define PHY_DEVICE      0x10
#define LOGICAL_DEVICE  0x01
#define FLAG            0x7E
#define TYPE3           0x0A
#define MAX_INFO_FIELD  0x80
#define MIN_FRAME_SIZE  10      /* flag 1 + format 2 + address 3 + control 1 + FCS 2 + flag = 10 byte */
#define SNRM            0x93
#define DISC            0x53
#define UA              0x73
#define LSAP            0xe6
#define CMD_LSAP        LSAP
#define RESP_LSAP       0xe7
#define AARQ            0x60
#define AARE            0x61
#define AUTH            0xac
#define GET_REQUEST     0xc0
#define GET_RESPONSE    0xc4

enum {
    TYPE_SIGNED_32    = 0x05,
    TYPE_UNSIGNED_32  = 0x06,
    TYPE_OCTET_STRING = 0x09
};

typedef struct __attribute__((packed)) {
    uint16_t    length              :11;    /* frame length                             */
    uint16_t    segmentation        :1;     /* 1 - segmentation, 0 - no segmentation    */
    uint16_t    type                :4;     /* 0x0A - type 3                            */
} format_t;

typedef struct __attribute__((packed)) {
    uint8_t     flag;                       /* 0x7E                                     */
    uint8_t     format[2];                  /* type, segmentation and length            */
    uint8_t     addr[3];                    /* addr0+addr1 addr2 or addr0 addr1+addr2   */
    uint8_t     control;                    /* control - srnm, disc, etc.               */
} header_t;

typedef struct __attribute__((packed)) {
    header_t    header;                     /* | FLAG | Format | Dest addr | Src addr | Control |   */
    uint8_t     data[PKT_BUFF_MAX_LEN*2];   /* | HCS | Information | FCS | Flag |                   */
} package_t;

typedef struct __attribute__((packed)) {
    uint8_t     clazz[2];
    uint8_t     obis[6];
    uint8_t     attribute[2];
} request_t;

typedef struct __attribute__((packed)) {
    uint8_t     type;
    uint8_t     size;
    uint8_t     str;
} type_octet_string_t;

typedef struct __attribute__((packed)) {
    uint8_t     type;                       /* 0x05 or 0x06 */
    uint32_t    value;
} type_digit_t; /* int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t */

typedef struct __attribute__((packed)) {
    uint8_t     client_addr;
    uint16_t    server_lower_addr;
    uint16_t    server_upper_addr;
    uint8_t     password[8];
    format_t    format;
    uint16_t    max_info_field_tx;
    uint16_t    max_info_field_rx;
    uint32_t    window_tx;
    uint32_t    window_rx;
    uint8_t     rrr;
    uint8_t     sss;
} meter_t;

typedef struct __attribute__((packed)) {
    size_t      size;
    uint8_t     complete;                   /* 1 - complete, 0 - not complete */
    uint8_t     buff[UART_BUFF_SIZE];
} result_package_t;


static request_t attr_descriptor_serial_number = {
    .clazz =     {0x00, 0x01},
    .obis =      {0x00, 0x00, 0x60, 0x01, 0x00, 0xff},   /* 0.0.96.01.00.255 */
    .attribute = {0x02, 0x00}
};

static request_t attr_descriptor_date_release = {
    .clazz =     {0x00, 0x01},
    .obis =      {0x00, 0x00, 0x60, 0x01, 0x04, 0xff},   /* 0.0.96.1.4.255   */
    .attribute = {0x02, 0x00}
};

static request_t attr_descriptor_list = {
    .clazz  =    {0x00, 0x07},
    .obis   =    {0x01, 0x00, 0x5e, 0x07, 0x00, 0xff},   /* 1.0.94.7.0.255   */
    .attribute = {0x02, 0x00}
};

static meter_t meter;


class Command {
public:
  Command(const std::string &name, bool has_response = true, bool on_start = false, uint8_t publish_size = 0, std::function<void(uint8_t number)> on_publish = [](uint8_t) { return true; })
    : name_(name), has_response_(has_response), on_start_(on_start), publish_size_(publish_size), on_publish_(on_publish) {}
  std::string& get_name() { return this->name_; }
  bool is_on_start() { return this->on_start_; }
  bool has_response() { return this->has_response_; }
  int fill_notification_request(package_t *package);
  virtual int fill_request(package_t *package) = 0;
  virtual bool process_result(header_t *header, result_package_t *package) = 0;
  bool publish_result();
  static uint16_t checksum(const uint8_t *src_buffer, size_t len);
  static uint8_t set_address(uint8_t *buff, uint8_t len, uint16_t lower, uint16_t upper);
  static uint8_t get_address(uint8_t *buff, uint8_t len, uint16_t *lower, uint16_t *upper);
  static uint8_t get_address_size(uint8_t *buff);
  static uint32_t reverse32(uint32_t in);
protected:
  size_t set_header(package_t *raw_package);
  int request_data(request_t *request, package_t *raw_package);
private:
  uint8_t publish_size_, publish_counter_{0};
  std::function<void(uint8_t counter)> on_publish_;
  std::string name_;
  bool has_response_, on_start_;
};

class CommandSNRM : public Command {
public:
  CommandSNRM() : Command("snrm") {}
  int fill_request(package_t *package) override;
  bool process_result(header_t *header, result_package_t *package) override { return header->control == UA; }
};

class CommandOpenSession : public Command {
public:
  CommandOpenSession() : Command("open_session") {}
  int fill_request(package_t *package) override;
  bool process_result(header_t *header, result_package_t *package) override;
};

class CommandDisconnect : public Command {
public:
  CommandDisconnect() : Command("disconnect", false) {}
  int fill_request(package_t *package) override;
  bool process_result(header_t *header, result_package_t *package) override { return true; };
};

class CommandGetSerialNumber : public Command {
public:
  CommandGetSerialNumber(std::function<void(const std::string&)> on_value) : Command("get_serial_number", true, true, 1, [this](uint8_t counter) {on_value_(std::to_string(number));}), on_value_(on_value) {}
  int fill_request(package_t *package) override { return request_data(&attr_descriptor_serial_number, package); }
  bool process_result(header_t *header, result_package_t *package) override;
private:
  uint32_t number;
  std::function<void(const std::string &number)> on_value_;
};

class CommandGetReleaseDate : public Command {
public:
  CommandGetReleaseDate(std::function<void(const std::string&)> on_value) : Command("get_release_date", true, true, 1, [this](uint8_t counter) {on_value_(date);}), on_value_(on_value) {}
  int fill_request(package_t *package) override { return request_data(&attr_descriptor_date_release, package); }
  bool process_result(header_t *header, result_package_t *package) override;
private:
  std::string date;
  std::function<void(const std::string &number)> on_value_;
};

class CommandGetListData : public Command {
public:
  CommandGetListData(std::function<void(uint8_t, float, float, float, float, float, float, float)> on_value)
    : Command("get_list_data", true, false, 7, [this](uint8_t counter) {on_value_(counter, cvp[0], cvp[1], cvp[2], energy[0], energy[1], energy[2], energy[3]);}), on_value_(on_value) {}
  int fill_request(package_t *package) override { return request_data(&attr_descriptor_list, package); }
  bool process_result(header_t *header, result_package_t *package) override;
private:
  std::function<void(uint8_t counter, float c, float v, float p, float t1, float t2, float t3,float t4)> on_value_;
  float energy[MAX_TARIFF_COUNT], cvp[3];
};


class Nartis100 : public PollingComponent, public uart::UARTDevice {
public:
  Nartis100(uart::UARTComponent *uart, const std::string &password);
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;
  void set_startup_delay(uint32_t startup_delay) { this->startup_delay_ = startup_delay; }
  void set_dir_pin(GPIOPin *pin) { this->dir_pin_ = pin; }
  void set_current_sensor(sensor::Sensor *sensor) { this->sensor_current_ = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->sensor_voltage_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->sensor_power_ = sensor; }
  void set_error_binary_sensor(binary_sensor::BinarySensor *sensor) { this->sensor_error_ = sensor; }
  void set_energy_sensor(uint16_t idx, sensor::Sensor *sensor) { this->sensor_energy_[idx] = sensor; }
  void set_serial_number_sensor(text_sensor::TextSensor *sensor) { this->sensor_serial_number_ = sensor; }
  void set_release_date_sensor(text_sensor::TextSensor *sensor) { this->sensor_release_date_ = sensor; }

protected:
  void delay(uint32_t ms) { this->sleep_time_ = millis() + ms; }
  uint16_t crc16(const uint8_t *data, uint16_t len);
  std::vector<Command *> commands_;

private:
  uint32_t startup_delay_{0}, phase_{0};
  unsigned long sleep_time_{0}, wait_time_{0};
  uint8_t *rx_buffer_, *tx_buffer_;
  package_t rx_package_, tx_package_;
  result_package_t result_package_;
  uint16_t tx_bytes_length_{0}, tx_bytes_sent_{0}, rx_bytes_needed_{0}, rx_bytes_received_{0};
  bool error_{false}, started_{false};
  GPIOPin *dir_pin_{nullptr};
  binary_sensor::BinarySensor *sensor_error_{nullptr};
  sensor::Sensor *sensor_current_{nullptr}, *sensor_voltage_{nullptr}, *sensor_power_{nullptr};
  sensor::Sensor *sensor_energy_[MAX_TARIFF_COUNT];
  text_sensor::TextSensor *sensor_serial_number_{nullptr}, *sensor_release_date_{nullptr};
  format_t format;
};

template <typename... Ts> class Nartis100ForceUpdateAction : public Action<Ts...> {
public:
  Nartis100ForceUpdateAction(Nartis100 *parent) : parent_(parent) {}
  void play(Ts... x) override { parent_->update(); }
private:
  Nartis100 *parent_;
};

} // namespace nartis100
} // namespace esphome
