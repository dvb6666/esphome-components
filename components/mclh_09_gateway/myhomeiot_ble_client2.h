#pragma once
#ifndef MYHOMEIOT_BLE_CLIENT_2
#define MYHOMEIOT_BLE_CLIENT_2

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/myhomeiot_ble_host/myhomeiot_ble_host.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace myhomeiot_ble_client2 {

static const char *const TAG = "myhomeiot_ble_client2";

class MyHomeIOT_BLEClientService {
public:
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID char_uuid_;
  uint16_t start_handle_;
  uint16_t end_handle_;
  uint16_t char_handle_;
  bool processed;

  bool is_write() { return this->type == 1; }
  bool is_notify() { return this->type == 2; }
  bool is_processed() { return this->processed; }
  void set_processed() { this->processed = true; }
  void reset() {
    this->start_handle_ = this->end_handle_ = this->char_handle_ = ESP_GATT_ILLEGAL_HANDLE;
    processed = false;
  }
  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid); }
  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid); }
  void set_notify() { this->type = 2; }
  void set_skip_empty() { this->skip_empty_ = true; }
  bool get_skip_empty() { return this->skip_empty_; }

  void set_delay_simple(uint32_t value) {
    this->delay_simple_ = value;
    this->has_simple_delay_ = true;
  }
  void set_delay_template(std::function<uint32_t()> func) {
    this->delay_template_ = std::move(func);
    this->has_simple_delay_ = false;
  }
  uint32_t get_delay() { return this->has_simple_delay_ ? this->delay_simple_ : this->delay_template_(); }

  void set_value_simple(const std::vector<uint8_t> &value) {
    this->type = 1;
    this->value_simple_ = value;
    this->has_simple_value_ = true;
  }
  void set_value_template(std::function<std::vector<uint8_t>()> func) {
    this->type = 1;
    this->value_template_ = std::move(func);
    this->has_simple_value_ = false;
  }
  std::vector<uint8_t> get_value() { return this->has_simple_value_ ? this->value_simple_ : this->value_template_(); }

private:
  uint8_t type = 0;
  bool skip_empty_ = false;
  bool has_simple_delay_ = true;
  uint32_t delay_simple_ = 0;
  std::function<uint32_t()> delay_template_{};
  bool has_simple_value_ = true;
  std::vector<uint8_t> value_simple_;
  std::function<std::vector<uint8_t>()> value_template_{};
};

class MyHomeIOT_BLEClient2 : public PollingComponent, public myhomeiot_ble_host::MyHomeIOT_BLEClientNode {
public:
  void setup() override {
    this->state_ = MYHOMEIOT_IDLE;
    for (auto *service : this->services)
      if (service->is_notify())
        this->has_notify = true;
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "MyHomeIOT BLE Client V2");
    ESP_LOGCONFIG(TAG, "  MAC address: %s", to_string(this->address_).c_str());
    if (this->services.empty())
      ESP_LOGW(TAG, "  No one service defined! Device is undiscovered.");
    else {
      for (int i = 0; i < this->services.size(); i++)
        ESP_LOGCONFIG(TAG, "  %d) %s Service UUID: %s,  Characteristic UUID: %s", i + 1,
                      this->services[i]->is_notify()  ? "Notify"
                      : this->services[i]->is_write() ? "Write"
                                                      : "Read",
                      this->services[i]->service_uuid_.to_string().c_str(), this->services[i]->char_uuid_.to_string().c_str());
      LOG_UPDATE_INTERVAL(this);
    }
  }

  void loop() override {
    if (this->state_ == MYHOMEIOT_DISCOVERED)
      this->connect();
    else if (this->state_ == MYHOMEIOT_ESTABLISHED)
      this->disconnect();
    else if (this->state_ == MYHOMEIOT_CONNECTED && this->wait_until > 0 && this->wait_until < millis()) {
      ESP_LOGD(TAG, "[%s] Time to wake up", to_string(this->address_).c_str());
      process_next_service();
    }
  }

  void add_on_connect_callback(std::function<void(int, const MyHomeIOT_BLEClient2 &)> &&callback) {
    this->connect_callback_.add(std::move(callback));
  }

  void add_on_value_callback(std::function<void(std::vector<uint8_t>, int, bool &, const MyHomeIOT_BLEClient2 &)> &&callback) {
    this->value_callback_.add(std::move(callback));
  }

  void add_on_error_callback(std::function<void(int32_t, const MyHomeIOT_BLEClient2 &)> &&callback) {
    this->error_callback_.add(std::move(callback));
  }

  void force_update() {
    if (this->state_ == MYHOMEIOT_IDLE) {
      ESP_LOGD(TAG, "[%s] Force update requested", to_string(this->address_).c_str());
      this->is_update_requested_ = true;
    }
  }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (!this->is_update_requested_ || this->state_ != MYHOMEIOT_IDLE || device.address_uint64() != this->address_ || services.empty())
      return false;

    ESP_LOGD(TAG, "[%s] Found device. RSSI: %d", device.address_str().c_str(), device.get_rssi());
    memcpy(this->remote_bda_, device.address(), sizeof(this->remote_bda_));
    this->state_ = MYHOMEIOT_DISCOVERED;
    this->rssi_ = device.get_rssi();
    return true;
  }

  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (memcmp(param->open.remote_bda, this->remote_bda_, sizeof(this->remote_bda_)) != 0)
        break;
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] OPEN_EVT failed, status (%d), app_id (%d)", to_string(this->address_).c_str(), param->open.status,
                 ble_host_->app_id);
        report_error(MYHOMEIOT_IDLE);
        break;
      }
      ESP_LOGI(TAG, "[%s] Connected successfully, app_id (%d)", to_string(this->address_).c_str(), ble_host_->app_id);
      this->conn_id_ = param->open.conn_id;
      if (auto status = esp_ble_gattc_send_mtu_req(ble_host_->gattc_if, param->open.conn_id)) {
        ESP_LOGW(TAG, "[%s] send_mtu_req failed, status (%d)", to_string(this->address_).c_str(), status);
        report_error();
        break;
      }
      reset_client();
      this->state_ = MYHOMEIOT_CONNECTED;
      this->connect_callback_.call(this->rssi_, *this);
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.conn_id != this->conn_id_)
        break;
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] CFG_MTU_EVT failed, status (%d)", to_string(this->address_).c_str(), param->cfg_mtu.status);
        report_error();
        break;
      }
      ESP_LOGV(TAG, "[%s] CFG_MTU_EVT, MTU (%d)", to_string(this->address_).c_str(), param->cfg_mtu.mtu);
      if (auto status = esp_ble_gattc_search_service(esp_gattc_if, param->cfg_mtu.conn_id, nullptr)) {
        ESP_LOGW(TAG, "[%s] search_service failed, status (%d)", to_string(this->address_).c_str(), status);
        report_error();
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (memcmp(param->disconnect.remote_bda, this->remote_bda_, sizeof(this->remote_bda_)) != 0)
        return false;
      ESP_LOGD(TAG, "[%s] DISCONNECT_EVT", to_string(this->address_).c_str());
      this->state_ = MYHOMEIOT_IDLE;
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      if (param->search_res.conn_id != this->conn_id_)
        break;
      esp32_ble_tracker::ESPBTUUID uuid = param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16
                                              ? esp32_ble_tracker::ESPBTUUID::from_uint16(param->search_res.srvc_id.uuid.uuid.uuid16)
                                          : param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_32
                                              ? esp32_ble_tracker::ESPBTUUID::from_uint32(param->search_res.srvc_id.uuid.uuid.uuid32)
                                              : esp32_ble_tracker::ESPBTUUID::from_raw(param->search_res.srvc_id.uuid.uuid.uuid128);
      bool found = false;
      for (int i = 0; i < this->services.size(); i++)
        if (uuid == this->services[i]->service_uuid_) {
          ESP_LOGD(TAG, "[%s] SEARCH_RES_EVT service[%d] (%s) found", to_string(this->address_).c_str(), i + 1, uuid.to_string().c_str());
          this->services[i]->start_handle_ = param->search_res.start_handle;
          this->services[i]->end_handle_ = param->search_res.end_handle;
          found = true;
        }
      if (!found)
        ESP_LOGV(TAG, "[%s] SEARCH_RES_EVT got unused service (%s)", to_string(this->address_).c_str(), uuid.to_string().c_str());
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->conn_id_)
        break;
      ESP_LOGV(TAG, "[%s] SEARCH_CMPL_EVT", to_string(this->address_).c_str());

      for (int i = 0; i < this->services.size(); i++)
        if (this->services[i]->start_handle_ == ESP_GATT_ILLEGAL_HANDLE) {
          ESP_LOGW(TAG, "[%s] SEARCH_CMPL_EVT service[%d] (%s) not found", to_string(this->address_).c_str(), i + 1,
                   this->services[i]->service_uuid_.to_string().c_str());
        }
      process_next_service();
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->conn_id_ || param->read.handle != this->services[processing_service]->char_handle_) {
        ESP_LOGD(TAG, "[%s] Received read for unknown handle (%d)", to_string(this->address_).c_str(), param->read.handle);
        break;
      }
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] READ_CHAR_EVT error reading char at handle (%d), status (%d)", to_string(this->address_).c_str(),
                 param->read.handle, param->read.status);
        report_error();
        break;
      }

      report_results(param->read.value, param->read.value_len, this->processing_service);
      process_next_service();
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] WRITE_CHAR_EVT error writing char at handle (%d), status (%d)", to_string(this->address_).c_str(),
                 param->write.handle, param->write.status);
        report_error();
        break;
      }
      ESP_LOGD(TAG, "[%s] WRITE_CHAR offset: %d", to_string(this->address_).c_str(), param->write.offset);
      process_next_service();
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] REG_FOR_NOTIFY_EVT error at handle (%d), status (%d)", to_string(this->address_).c_str(),
                 param->reg_for_notify.handle, param->reg_for_notify.status);
        report_error();
        break;
      }
      ESP_LOGD(TAG, "[%s] Reg notify event", to_string(this->address_).c_str());
      process_next_service();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->conn_id_)
        break;
      for (int i = 0; i < this->services.size(); i++)
        if (this->services[i]->is_notify() && this->services[i]->char_handle_ == param->notify.handle) {
          report_results(param->notify.value, param->notify.value_len, i);
          process_next_service();
          return true;
        }
      ESP_LOGD(TAG, "[%s] Received notify for unknown handle (%d)", to_string(this->address_).c_str(), param->notify.handle);
      break;
    }
    default:
      ESP_LOGV(TAG, "[%s] Non processed event type: %d", to_string(this->address_).c_str(), event);
      break;
    }
    return true;
  }

  void set_address(uint64_t address) { this->address_ = address; }
  float get_setup_priority() const override { return setup_priority::DATA; }
  const uint8_t *remote_bda() const { return remote_bda_; }
  void add_service(MyHomeIOT_BLEClientService *service) { this->services.push_back(service); }

protected:
  std::string to_string(uint64_t address) const {
    char buffer[20];
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)(address >> 40), (uint8_t)(address >> 32), (uint8_t)(address >> 24),
            (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address >> 0));
    return std::string(buffer);
  }

  CallbackManager<void(int, const MyHomeIOT_BLEClient2 &)> connect_callback_{};
  CallbackManager<void(std::vector<uint8_t>, int, bool &, const MyHomeIOT_BLEClient2 &)> value_callback_{};
  CallbackManager<void(int32_t, const MyHomeIOT_BLEClient2 &)> error_callback_{};
  bool is_update_requested_;
  uint64_t address_;
  esp_bd_addr_t remote_bda_;
  int rssi_ = 0;
  uint32_t error_count_ = 0;
  uint16_t conn_id_;
  int processing_service;
  std::vector<MyHomeIOT_BLEClientService *> services;
  bool stop_processing = false;
  bool has_notify = false;
  uint32_t wait_until = 0;
  char temp_str[65];

  void connect() {
    ESP_LOGI(TAG, "[%s] Connecting", to_string(this->address_).c_str());
    this->state_ = MYHOMEIOT_CONNECTING;
    if (auto status = esp_ble_gattc_open(ble_host_->gattc_if, this->remote_bda_, BLE_ADDR_TYPE_PUBLIC, true)) {
      ESP_LOGW(TAG, "[%s] open error, status (%d)", to_string(this->address_).c_str(), status);
      report_error(MYHOMEIOT_IDLE);
    }
  }

  void disconnect() {
    ESP_LOGI(TAG, "[%s] Disconnecting", to_string(this->address_).c_str());
    this->state_ = MYHOMEIOT_IDLE;
    if (auto status = esp_ble_gattc_close(ble_host_->gattc_if, this->conn_id_))
      ESP_LOGW(TAG, "[%s] close error, status (%d)", to_string(this->address_).c_str(), status);
  }

  void update() override { this->is_update_requested_ = true; }

  void report_results(uint8_t *data, uint16_t len, int service) {
    this->status_clear_warning();
    std::vector<uint8_t> value(data, data + len);
    vec2str(value);
    ESP_LOGD(TAG, "[%s] Receiving %d bytes for %sservice[%d] (%s): [0x%s]", to_string(this->address_).c_str(), value.size(),
             this->services[service]->is_notify() ? "notify " : "", service + 1, this->services[service]->service_uuid_.to_string().c_str(),
             temp_str);
    this->value_callback_.call(value, service + 1, this->stop_processing, *this);
    if (this->stop_processing) {
      ESP_LOGD(TAG, "[%s] Stop processing after service[%d] (%s)", to_string(this->address_).c_str(), service + 1,
               this->services[service]->service_uuid_.to_string().c_str());
      this->processing_service = this->services.size() + 1;
    }
  }

  void report_error(esp32_ble_tracker::ClientState state = MYHOMEIOT_ESTABLISHED) {
    this->state_ = state;
    this->status_set_warning();
    this->error_callback_.call(++(this->error_count_), *this);
  }

  void reset_client() {
    this->stop_processing = false;
    this->processing_service = 0;
    this->wait_until = 0;
    for (auto *service : this->services)
      service->reset();
  }

  void vec2str(const std::vector<uint8_t> &value) {
    strcpy(temp_str, "");
    for (int i = 0; i < value.size() && i < sizeof(temp_str) / 2; i++)
      snprintf(temp_str + 2 * i, sizeof(temp_str - 2 * i), "%02X", value[i]);
  }

  bool process_next_service() {
    for (; true; this->processing_service++) {

      if (this->stop_processing || this->processing_service >= this->services.size()) {
        this->status_clear_warning();
        if (!this->stop_processing && this->has_notify) {
          ESP_LOGD(TAG, "[%s] All services processed, but need to wait notifies", to_string(this->address_).c_str());
          return false;
        }
        ESP_LOGV(TAG, "[%s] All services processed", to_string(this->address_).c_str());
        this->is_update_requested_ = false;
        this->state_ = MYHOMEIOT_ESTABLISHED;
        return false;
      }

      if (this->services[this->processing_service]->is_processed() ||
          this->services[this->processing_service]->start_handle_ == ESP_GATT_ILLEGAL_HANDLE)
        continue;

      if (this->wait_until > 0 && this->wait_until > millis())
        return false;

      if (this->wait_until == 0) {
        uint32_t delay = this->services[this->processing_service]->get_delay();
        if (delay > 0) {
          ESP_LOGD(TAG, "[%s] Waiting for %dms for service[%d] (%s)", to_string(this->address_).c_str(), delay,
                   this->processing_service + 1, this->services[this->processing_service]->service_uuid_.to_string().c_str());
          this->wait_until = millis() + delay;
          return false;
        }
      }

      this->wait_until = 0;
      this->services[this->processing_service]->set_processed();

      if (process_service(this->processing_service))
        return true;

      continue;
    }
  }

  bool process_service(int i) {
    std::vector<uint8_t> value;

    if (this->services[i]->is_write()) {
      value = services[i]->get_value();
      if (value.empty() && this->services[i]->get_skip_empty()) {
        ESP_LOGD(TAG, "[%s] Skip service[%d] (%s) cause nothing to send", to_string(this->address_).c_str(), i + 1,
                 this->services[i]->service_uuid_.to_string().c_str());
        return false;
      }
    }

    uint16_t offset = 0;
    esp_gattc_char_elem_t result;
    while (true) {
      uint16_t count = 1;
      auto status = esp_ble_gattc_get_all_char(ble_host_->gattc_if, this->conn_id_, this->services[i]->start_handle_,
                                               this->services[i]->end_handle_, &result, &count, offset);
      if (status != ESP_GATT_OK) {
        if (status == ESP_GATT_INVALID_OFFSET || status == ESP_GATT_NOT_FOUND)
          break;
        ESP_LOGW(TAG, "[%s] get_all_char error, status (%d)", to_string(this->address_).c_str(), status);
        report_error();
        break;
      }
      if (count == 0)
        break;

      if (this->services[i]->char_uuid_ == esp32_ble_tracker::ESPBTUUID::from_uuid(result.uuid)) {
        ESP_LOGD(TAG, "[%s] SEARCH_CMPL_EVT char (%s) found", to_string(this->address_).c_str(),
                 this->services[i]->char_uuid_.to_string().c_str());
        this->services[i]->char_handle_ = result.char_handle;

        if (this->services[i]->is_notify()) {
          auto status = esp_ble_gattc_register_for_notify(ble_host_->gattc_if, this->remote_bda_, this->services[i]->char_handle_);
          if (status != ESP_GATT_OK) {
            ESP_LOGW(TAG, "[%s] register_for_notify failed, status (%d)", to_string(this->address_).c_str(), status);
            this->services[i]->char_handle_ = ESP_GATT_ILLEGAL_HANDLE;
          }

        } else if (this->services[i]->is_write()) {
          esp_gatt_write_type_t write_type;
          if (result.properties & ESP_GATT_CHAR_PROP_BIT_WRITE) {
            write_type = ESP_GATT_WRITE_TYPE_RSP;
          } else if (result.properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) {
            write_type = ESP_GATT_WRITE_TYPE_NO_RSP;
          } else {
            ESP_LOGE(TAG, "[%s] Characteristic %s does not allow writing", to_string(this->address_).c_str(),
                     this->services[i]->char_uuid_.to_string().c_str());
            this->services[i]->char_handle_ = ESP_GATT_ILLEGAL_HANDLE;
            break;
          }

          vec2str(value);
          ESP_LOGD(TAG, "[%s] Sending %d bytes %sfor service[%d] (%s): [%s%s]", to_string(this->address_).c_str(), value.size(),
                   write_type == ESP_GATT_WRITE_TYPE_RSP ? "(with response) " : "", i + 1,
                   this->services[i]->service_uuid_.to_string().c_str(), strlen(temp_str) > 0 ? "0x" : "", temp_str);

          auto status = esp_ble_gattc_write_char(ble_host_->gattc_if, this->conn_id_, this->services[i]->char_handle_, value.size(),
                                                 value.data(), write_type, ESP_GATT_AUTH_REQ_NONE);
          if (status != ESP_GATT_OK) {
            ESP_LOGW(TAG, "[%s] write_char error sending write request, status (%d)", to_string(this->address_).c_str(), status);
            this->services[i]->char_handle_ = ESP_GATT_ILLEGAL_HANDLE;
          }

        } else {
          auto status =
              esp_ble_gattc_read_char(ble_host_->gattc_if, this->conn_id_, this->services[i]->char_handle_, ESP_GATT_AUTH_REQ_NONE);
          if (status != ESP_GATT_OK) {
            ESP_LOGW(TAG, "[%s] read_char error sending read request, status (%d)", to_string(this->address_).c_str(), status);
            this->services[i]->char_handle_ = ESP_GATT_ILLEGAL_HANDLE;
          }
        }
        break;
      }
      offset++;
    }
    if (this->services[i]->char_handle_ == ESP_GATT_ILLEGAL_HANDLE) {
      ESP_LOGE(TAG, "[%s] SEARCH_CMPL_EVT char (%s) not found", to_string(this->address_).c_str(),
               this->services[i]->char_uuid_.to_string().c_str());
      report_error();
    }
    return true;
  }
};

class MyHomeIOT_BLEClientConnectTrigger : public Trigger<int, const MyHomeIOT_BLEClient2 &> {
public:
  explicit MyHomeIOT_BLEClientConnectTrigger(MyHomeIOT_BLEClient2 *parent) {
    parent->add_on_connect_callback([this](int rssi, const MyHomeIOT_BLEClient2 &xthis) { this->trigger(rssi, xthis); });
  }
};

class MyHomeIOT_BLEClientValueTrigger : public Trigger<std::vector<uint8_t>, int, bool &, const MyHomeIOT_BLEClient2 &> {
public:
  explicit MyHomeIOT_BLEClientValueTrigger(MyHomeIOT_BLEClient2 *parent) {
    parent->add_on_value_callback([this](std::vector<uint8_t> value, int service, bool &stop_processing,
                                         const MyHomeIOT_BLEClient2 &xthis) { this->trigger(value, service, stop_processing, xthis); });
  }
};

class MyHomeIOT_BLEClientErrorTrigger : public Trigger<uint32_t, const MyHomeIOT_BLEClient2 &> {
public:
  explicit MyHomeIOT_BLEClientErrorTrigger(MyHomeIOT_BLEClient2 *parent) {
    parent->add_on_error_callback([this](uint32_t error_count, const MyHomeIOT_BLEClient2 &xthis) { this->trigger(error_count, xthis); });
  }
};

template <typename... Ts> class MyHomeIOT_BLEClientForceUpdateAction : public Action<Ts...> {
public:
  MyHomeIOT_BLEClientForceUpdateAction(MyHomeIOT_BLEClient2 *ble_client) : ble_client_(ble_client) {}
  void play(Ts... x) override { ble_client_->force_update(); }

private:
  MyHomeIOT_BLEClient2 *ble_client_;
};

} // namespace myhomeiot_ble_client2
} // namespace esphome

#endif
#endif
