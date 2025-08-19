#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <queue>
#include <vector>

#ifdef USE_ESP32
#include <AsyncTCP.h>
#elif USE_ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#else
#error Unsupported platform
#endif

namespace esphome {
namespace modbus_tcp {

static const char *TAG = "modbus-tcp";

#define PROTOCOL_ID 0x0000
#define MAX_TX_BUFFER_SIZE 32
#define MAX_RX_BUFFER_SIZE 64

enum DebugDirection { DIRECTION_RX, DIRECTION_TX };

typedef struct {
  uint8_t function, response_code;
  uint16_t data1 = 0, data2 = 0;
  uint16_t phase = 0, tx_id = 0, length = 0;
} ModbusTcpCommand;

typedef std::function<void()> ModbusTcpOnConnect;
typedef std::function<void()> ModbusTcpOnDisconnect;
typedef std::function<void(uint8_t code, uint16_t error)> ModbusTcpOnError;
typedef std::function<void(uint8_t code, uint8_t *data, uint16_t register_count)> ModbusTcpOnRegisterResponse;

class ModbusTcpComponent : public Component {
 public:
  ModbusTcpComponent(const std::string &ip_address, uint16_t port, uint8_t address)
      : ip_address_(ip_address), port_(port), address_(address) {
    client_ = new AsyncClient();
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; };

  void setup() override {
    client_->onConnect([this](void *arg, AsyncClient *client) { on_connect(client); });
    client_->onDisconnect([this](void *arg, AsyncClient *client) { on_disconnect(client); });
    client_->onData([this](void *arg, AsyncClient *client, void *data, size_t len) { on_data(client, data, len); });
    start();
  };

  void loop() override {
    unsigned long current_time;
    if (this->sleep_time_ != 0 && this->sleep_time_ > (current_time = millis()))
      return;
    else if (this->sleep_time_ != 0)
      this->sleep_time_ = 0;

    // check if need to connect
    if (!connecting_ && !connected_ && can_connect_) {
      connect();
    }
    // check if we have next command
    else if (connected_ && !commands_queue_.empty()) {
      auto &command = this->commands_queue_.front();
      if (command == nullptr || this->process_command(command.get())) {
        this->commands_queue_.pop();
      }
    }
  };

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Modbus TCP: %s:%d", ip_address_.c_str(), port_);
    ESP_LOGCONFIG(TAG, "  Address: %d", address_);
  };

  void on_shutdown() override {
    if (connected_)
      client_->close(true);
  };

  void start() {
    ESP_LOGD(TAG, "Starting");
    can_connect_ = true;
  }

  void stop() {
    ESP_LOGD(TAG, "Stopping");
    can_connect_ = false;
    connect_attempt_ = 0;
    client_->stop();
  }

  bool is_started() { return can_connect_; }
  uint8_t get_address() { return address_; }
  void set_address(uint8_t address) { address_ = address; }
  void set_delay_after_connect(uint32_t ms) { delay_after_connect_ = ms; }
  void set_timeout(uint32_t ms) { timeout_ = ms; }
  void set_on_connect(ModbusTcpOnConnect handler) { on_connect_ = handler; }
  void set_on_disconnect(ModbusTcpOnDisconnect handler) { on_disconnect_ = handler; }
  void set_on_error(ModbusTcpOnError handler) { on_error_ = handler; }
  void set_on_register_response(ModbusTcpOnRegisterResponse handler) { on_register_response_ = handler; }

  bool read_registers(uint8_t response_code, uint16_t first_register, uint16_t registers_count = 1) {
    if (connected_) {
      uint16_t rx_size = 9 + 2 * registers_count > MAX_RX_BUFFER_SIZE;
      if (rx_size > MAX_RX_BUFFER_SIZE) {
        ESP_LOGW(TAG, "Too small RX buffer (current size %d, needed %d)", MAX_RX_BUFFER_SIZE, rx_size);
        return false;
      }
      ESP_LOGV(TAG, "read_registers(%d, %04x, %04x)", response_code, first_register, registers_count);
      this->commands_queue_.push(make_unique<ModbusTcpCommand>(0x03, response_code, first_register, registers_count));
    }
    return connected_;
  }

  bool write_register(uint8_t response_code, uint16_t address_register, uint16_t value) {
    if (connected_) {
      ESP_LOGV(TAG, "write_register(%d, %04x, %04x)", response_code, address_register, value);
      this->commands_queue_.push(make_unique<ModbusTcpCommand>(0x06, response_code, address_register, value));
    }
    return connected_;
  }

 protected:
  void delay(uint32_t delay_ms) { this->sleep_time_ = millis() + delay_ms; }

  void connect() {
    ESP_LOGD(TAG, "Try to connect (attempt %d)", connect_attempt_);
    connecting_ = false;
    if (client_->connect(ip_address_.c_str(), port_)) {
      connecting_ = true;
    } else {
      ESP_LOGW(TAG, "Failed to connect");
      delay(3000);
      connecting_ = false;
      connect_attempt_++;
    }
  }

  void on_connect(AsyncClient *client) {
    ESP_LOGD(TAG, "Connected");
    delay(delay_after_connect_);
    connect_attempt_ = 0;
    connected_ = true;
    connecting_ = false;
    if (on_connect_)
      on_connect_();
  }

  void on_disconnect(AsyncClient *client) {
    if (can_connect_ && !connected_) {
      ESP_LOGW(TAG, "Failed to connect");
      connect_attempt_++;
      delay(1000);
    } else if (can_connect_) {
      ESP_LOGW(TAG, "Disconnected");
      delay(1000);
    } else {
      ESP_LOGD(TAG, "Disconnected");
    }
    client->close(true);
    commands_queue_ = {};
    connecting_ = false;
    connected_ = false;
    if (on_disconnect_)
      on_disconnect_();
  }

  void on_data(AsyncClient *client, void *bytes, size_t len) {
    uint8_t *buffer = (uint8_t *) bytes;
#ifdef USE_TCP_DEBUGGER
    std::vector<uint8_t> data;
    data.assign(buffer, buffer + len);
    log_hex(DIRECTION_RX, data, ' ');
#endif
    ESP_LOGV(TAG, "Received %d bytes", len);
    for (size_t i = 0; i < len && rx_bytes_received_ < rx_bytes_needed_; i++)
      rx_buffer_[rx_bytes_received_++] = buffer[i];
  }

  bool process_command(ModbusTcpCommand *command) {
    ESP_LOGV(TAG, "Phase %d: command 0x%02X", command->phase, command->function);
    switch (command->phase) {
      case 0: {
        // prepare data to send
        command->tx_id = ++last_tx_id_;
        uint16_t request_length = 6;
        uint8_t buf_len = 0;
        tx_buffer_[buf_len++] = (command->tx_id >> 8) & 0xFF;  // Transaction Identifier
        tx_buffer_[buf_len++] = command->tx_id & 0xFF;
        tx_buffer_[buf_len++] = (PROTOCOL_ID >> 8) & 0xFF;  // Protocol Identifier always 0x0000
        tx_buffer_[buf_len++] = PROTOCOL_ID & 0xFF;
        tx_buffer_[buf_len++] = (request_length >> 8) & 0xFF;  // Message Length
        tx_buffer_[buf_len++] = request_length & 0xFF;
        tx_buffer_[buf_len++] = address_;           // Unit Identifier
        tx_buffer_[buf_len++] = command->function;  // Function Code
        // TODO refactor vor vector
        tx_buffer_[buf_len++] = (command->data1 >> 8) & 0xFF;
        tx_buffer_[buf_len++] = command->data1 & 0xFF;
        tx_buffer_[buf_len++] = (command->data2 >> 8) & 0xFF;
        tx_buffer_[buf_len++] = command->data2 & 0xFF;

#ifdef USE_TCP_DEBUGGER
        std::vector<uint8_t> data;
        data.assign((uint8_t *) tx_buffer_, (uint8_t *) tx_buffer_ + buf_len);
        log_hex(DIRECTION_TX, data, ' ');
#endif
        // send command
        if (!client_->write((char *) tx_buffer_, buf_len)) {
          ESP_LOGW(TAG, "Failed to send command 0x%02X", command->function);
          return true;
        }
        // calculate bytes to receive
        command->length = 2 + (command->function == 0x03 ? 1 + 2 * command->data2 : 4);
        rx_bytes_received_ = 0;
        rx_bytes_needed_ = 6 + command->length;
        receive_wait_time_ = millis() + timeout_;  // timeout N milliseconds
        delay(100);
      } break;

      case 1: {
        // wait complete packet?
        if (rx_bytes_received_ < rx_bytes_needed_) {
          if (receive_wait_time_ < millis()) {
            ESP_LOGW(TAG, "Timed out for command 0x%02X!", command->function);
            client_->stop();
            return true;
          }
          delay(5);  // wait some time for next bytes
          return false;
        }
      } break;

      case 2: {
        // validation
        uint8_t buf_len = 0;
        uint16_t tx_id = (uint16_t) rx_buffer_[buf_len++] << 8 | rx_buffer_[buf_len++];
        if (tx_id != command->tx_id) {
          ESP_LOGW(TAG, "Wrong transaction id for command 0x%02X (0x%04x instead of 0x%04x)", command->function, tx_id,
                   command->tx_id);
          return true;
        }
        uint16_t protocol = (uint16_t) rx_buffer_[buf_len++] << 8 | rx_buffer_[buf_len++];
        if (protocol != PROTOCOL_ID) {
          ESP_LOGW(TAG, "Wrong protocol 0x%04x for command 0x%02X", protocol, command->function);
          return true;
        }
        uint16_t length = (uint16_t) rx_buffer_[buf_len++] << 8 | rx_buffer_[buf_len++];
        if (length != command->length) {
          ESP_LOGW(TAG, "Wrong length for command 0x%02X (%d instead of %d)", command->function, length,
                   command->length);
          return true;
        }
        uint8_t address = rx_buffer_[buf_len++];
        if (address != address_) {
          ESP_LOGW(TAG, "Wrong address for command 0x%02X (0x%02x instead of 0x%02x)", command->function, address,
                   address_);
          return true;
        }
        uint8_t function = rx_buffer_[buf_len++];
        if ((function & 0x80) > 0) {
          ESP_LOGW(TAG, "Got error 0x%02X for command 0x%02X", rx_buffer_[buf_len], command->function);
          return true;
        } else if (function != command->function) {
          ESP_LOGW(TAG, "Wrong function code 0x%02X for command 0x%02X (0x%02x instead of 0x%02x)", function,
                   command->function);
          return true;
        }
        if (command->function == 0x03) {
          uint8_t data_size = rx_buffer_[buf_len++];
          if (data_size != command->length - 3) {
            ESP_LOGW(TAG, "Wrong response data size for command 0x%02X (%d instead of %d)", command->function,
                     data_size, command->length - 3);
            return true;
          }
        }
      } break;

      case 3: {
        if (on_register_response_) {
          uint16_t register_count = (command->function == 0x03 ? command->data2 : 1);
          ESP_LOGV(TAG, "Returning %d registers response for command 0x%02X, code 0x%02X", register_count,
                   command->function, command->response_code);
          on_register_response_(command->response_code, &rx_buffer_[rx_bytes_needed_ - 2 * register_count],
                                register_count);
        }
      } break;

      default:
        ESP_LOGV(TAG, "Command 0x%02X function completed", command->function);
        return true;
    }
    command->phase++;
    return false;
  }

  // copy from uart::UARTDebug
  static void log_hex(DebugDirection direction, std::vector<uint8_t> bytes, uint8_t separator) {
    std::string res;
    if (direction == DIRECTION_RX) {
      res += "<<< ";
    } else {
      res += ">>> ";
    }
    size_t len = bytes.size();
    char buf[5];
    for (size_t i = 0; i < len; i++) {
      if (i > 0) {
        res += separator;
      }
      sprintf(buf, "%02X", bytes[i]);
      res += buf;
    }
    ESP_LOGD(TAG, "%s", res.c_str());
  }

 private:
  std::string ip_address_;
  uint16_t port_;
  uint8_t address_;
  uint32_t delay_after_connect_, timeout_;
  AsyncClient *client_;
  ModbusTcpOnConnect on_connect_{0};
  ModbusTcpOnDisconnect on_disconnect_{0};
  ModbusTcpOnError on_error_{0};
  ModbusTcpOnRegisterResponse on_register_response_{0};
  std::queue<std::unique_ptr<ModbusTcpCommand>> commands_queue_;
  bool connecting_{false}, connected_{false}, can_connect_{false};
  unsigned long sleep_time_{0}, receive_wait_time_{0};
  int16_t connect_attempt_{0};
  uint16_t last_tx_id_{0};
  uint8_t tx_buffer_[MAX_TX_BUFFER_SIZE], rx_buffer_[MAX_RX_BUFFER_SIZE];
  uint16_t rx_bytes_needed_{0}, rx_bytes_received_{0};
};

}  // namespace modbus_tcp
}  // namespace esphome
