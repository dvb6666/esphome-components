#pragma once

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

  void reset() { this->start_handle_ = this->end_handle_ = this->char_handle_ = ESP_GATT_ILLEGAL_HANDLE; }
  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid); }
  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid); }
};

class MyHomeIOT_BLEClient2 : public PollingComponent, public myhomeiot_ble_host::MyHomeIOT_BLEClientNode {
public:
  void setup() override { this->state_ = MYHOMEIOT_IDLE; }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "MyHomeIOT BLE Client V2");
    ESP_LOGCONFIG(TAG, "  MAC address: %s", to_string(this->address_).c_str());
    if (services.empty())
      ESP_LOGW(TAG, "  No one service defined! Device is undiscovered.");
    else {
      for (int i = 0; i < services.size(); i++)
        ESP_LOGCONFIG(TAG, "  %d) Service UUID: %s,  Characteristic UUID: %s", i + 1, this->services[i]->service_uuid_.to_string().c_str(),
                      this->services[i]->char_uuid_.to_string().c_str());
      LOG_UPDATE_INTERVAL(this);
    }
  }

  void loop() override {
    if (this->state_ == MYHOMEIOT_DISCOVERED)
      this->connect();
    else if (this->state_ == MYHOMEIOT_ESTABLISHED)
      this->disconnect();
  }

  void add_on_state_callback(std::function<void(std::vector<uint8_t>, int, const MyHomeIOT_BLEClient2 &)> &&callback) {
    this->callback_.add(std::move(callback));
  }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (!this->is_update_requested_ || this->state_ != MYHOMEIOT_IDLE || device.address_uint64() != this->address_ || services.empty())
      return false;

    ESP_LOGD(TAG, "[%s] Found device", device.address_str().c_str());
    memcpy(this->remote_bda_, device.address(), sizeof(this->remote_bda_));
    this->state_ = MYHOMEIOT_DISCOVERED;
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
      reset_handles();
      this->state_ = MYHOMEIOT_CONNECTED;
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
      for (int i = 0; i < this->services.size(); i++)
        if (uuid == this->services[i]->service_uuid_) {
          ESP_LOGD(TAG, "[%s] SEARCH_RES_EVT service[%d] (%s) found", to_string(this->address_).c_str(), i + 1, uuid.to_string().c_str());
          this->services[i]->start_handle_ = param->search_res.start_handle;
          this->services[i]->end_handle_ = param->search_res.end_handle;
        }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->conn_id_)
        break;
      ESP_LOGV(TAG, "[%s] SEARCH_CMPL_EVT", to_string(this->address_).c_str());

      bool all_services_found = true;
      for (int i = 0; all_services_found && i < this->services.size(); i++)
        if (this->services[i]->start_handle_ == ESP_GATT_ILLEGAL_HANDLE) {
          ESP_LOGE(TAG, "[%s] SEARCH_CMPL_EVT service[%d] (%s) not found", to_string(this->address_).c_str(), i + 1,
                   this->services[i]->service_uuid_.to_string().c_str());
          all_services_found = false;
        }
      if (!all_services_found) {
        report_error();
        break;
      }

      process_next_service();
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->conn_id_ || param->read.handle != this->services[processing_service]->char_handle_)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] READ_CHAR_EVT error reading char at handle (%d), status (%d)", to_string(this->address_).c_str(),
                 param->read.handle, param->read.status);
        report_error();
        break;
      }

      report_results(param->read.value, param->read.value_len);
      if (!process_next_service())
        this->state_ = MYHOMEIOT_ESTABLISHED;
      break;
    }
    default:
      break;
    }
    return true;
  }

  void set_address(uint64_t address) { this->address_ = address; }
  float get_setup_priority() const override { return setup_priority::DATA; }
  const uint8_t *remote_bda() const { return remote_bda_; }

  void add_service(MyHomeIOT_BLEClientService *service) { this->services.push_back(service); }
  void add_service(uint16_t service_uuid, uint16_t char_uuid) {
    MyHomeIOT_BLEClientService *service = new MyHomeIOT_BLEClientService();
    service->set_service_uuid16(service_uuid);
    service->set_char_uuid16(char_uuid);
    this->services.push_back(service);
  }

protected:
  std::string to_string(uint64_t address) const {
    char buffer[20];
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)(address >> 40), (uint8_t)(address >> 32), (uint8_t)(address >> 24),
            (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address >> 0));
    return std::string(buffer);
  }

  CallbackManager<void(std::vector<uint8_t>, int, const MyHomeIOT_BLEClient2 &)> callback_{};
  bool is_update_requested_;
  uint64_t address_;
  esp_bd_addr_t remote_bda_;
  uint16_t conn_id_;
  int processing_service;
  std::vector<MyHomeIOT_BLEClientService *> services;

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

  void report_results(uint8_t *data, uint16_t len) {
    ESP_LOGV(TAG, "[%s] Reporting result for service (%s): %d", to_string(this->address_).c_str(),
             this->services[this->processing_service]->service_uuid_.to_string().c_str(), data[0]);
    this->status_clear_warning();
    std::vector<uint8_t> value(data, data + len);
    this->callback_.call(value, processing_service + 1, *this);
    this->is_update_requested_ = false;
  }

  void report_error(esp32_ble_tracker::ClientState state = MYHOMEIOT_ESTABLISHED) {
    this->state_ = state;
    this->status_set_warning();
  }

  void reset_handles() {
    this->processing_service = -1;
    for (int i = 0; i < this->services.size(); i++)
      this->services[i]->reset();
  }

  bool process_next_service() {
    if (++processing_service >= this->services.size())
      return false;

    int i = this->processing_service;
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

        if (auto status = esp_ble_gattc_read_char(ble_host_->gattc_if, this->conn_id_, this->services[i]->char_handle_,
                                                  ESP_GATT_AUTH_REQ_NONE) != ESP_GATT_OK) {
          ESP_LOGW(TAG, "[%s] read_char error sending read request, status (%d)", to_string(this->address_).c_str(), status);
          this->services[i]->char_handle_ = ESP_GATT_ILLEGAL_HANDLE;
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

class MyHomeIOT_BLEClientValueTrigger : public Trigger<std::vector<uint8_t>, int, const MyHomeIOT_BLEClient2 &> {
public:
  explicit MyHomeIOT_BLEClientValueTrigger(MyHomeIOT_BLEClient2 *parent) {
    parent->add_on_state_callback(
        [this](std::vector<uint8_t> value, int service, const MyHomeIOT_BLEClient2 &xthis) { this->trigger(value, service, xthis); });
  }
};

} // namespace myhomeiot_ble_client2
} // namespace esphome

#endif
