#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/myhomeiot_ble_client2/myhomeiot_ble_client2.h"
#include "esphome/components/myhomeiot_ble_host/myhomeiot_ble_host.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace mclh_09_mqtt_gateway {

// USE_ESP32_FRAMEWORK_ARDUINO
#ifdef USE_ESP_IDF
#define snprintf_P snprintf
#define strncat_P strncat
#define PSTR
#endif

const char *TAG = "mclh-09";
const char *ADVR_MODEL = "4D434C482D3039";

std::vector<float> temp_input{1035, 909, 668, 424, 368, 273, 159, 0};
std::vector<float> temp_output{68.8, 49.8, 24.3, 6.4, 1.0, -5.5, -20.5, -41.0};
std::vector<float> soil_input{1254, 1249, 1202, 1104, 944, 900};
std::vector<float> soil_output{60.0, 58.0, 54.0, 22.0, 2.0, 0.0};
std::vector<float> lumi_input{911, 764, 741, 706, 645, 545, 196, 117, 24, 17, 0};
std::vector<float> lumi_output{175300.0, 45400.0, 32100.0, 20300.0, 14760.0, 7600.0, 1200.0, 444.0, 29.0, 17.0, 0.0};

const char *available_suffix = "status", *state_suffix = "state", *alert_set_suffix = "alert/set", *alert_state_suffix = "alert";
const char *default_payload_online = "online", *default_payload_offline = "offline";

struct Sensor {
  const char *id, *name, *unit, *dev_class, *icon, *state_class;
  uint8_t accuracy;
  bool diag;
};
struct Select {
  const char *id, *name, *icon, *set_suffix, *state_suffix;
  bool diag, optimistic;
  std::vector<std::string> &options;
};

const char *MEASURE = "measurement", *HUMIDITY = "humidity", *SOIL2 = "soil";
Sensor BATT{"batt", "Battery", "%", "battery", NULL, MEASURE, 0, true};
Sensor TEMP{"temp", "Temperature", "°C", "temperature", NULL, MEASURE, 1, false};
Sensor LUMI{"lumi", "Illuminance", "lx", "illuminance", NULL, MEASURE, 0, false};
Sensor SOIL{"soil", "Soil moisture", "%", SOIL2, "mdi:water", MEASURE, 0, false};
Sensor HUMI{"humi", "Air humidity", "%", HUMIDITY, NULL, MEASURE, 0, false};
Sensor RSSI{"rssi", "RSSI", "dBm", "signal_strength", "mdi:signal", NULL, 0, true};
Sensor ERRORS{"errors", "Error count", NULL, NULL, "mdi:alert-circle", MEASURE, 0, true};

std::vector<std::string> alert_options{"Off",
                                       "Red (once)",
                                       "Green (once)",
                                       "Red + green (once)",
                                       "Red (every update)",
                                       "Green (every update)",
                                       "Red + green (every update)",
                                       "Green (always)"};
Select ALERT{"alert", "Alert", "mdi:alarm-light", alert_set_suffix, alert_state_suffix, false, false, alert_options};
std::vector<Sensor *> mclh09_sensors;
std::vector<Select *> mclh09_selects{&ALERT};

class Mclh09Device : public Component {
public:
  Mclh09Device(mqtt::MQTTClientComponent *mqtt_client, myhomeiot_ble_host::MyHomeIOT_BLEHost *ble_host, const std::string &topic_prefix,
               uint32_t update_interval, uint64_t address) {
    this->mqtt_client_ = mqtt_client;
    this->address_ = address;

    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("mclh09_%012llx"), address);
    this->id = std::string(temp_buffer);
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("MCLH-09 %06x"), (uint32_t)address & 0xFFFFFF);
    this->name = std::string(temp_buffer);

    const char *prefix = topic_prefix.c_str();
    const char *delimiter = (prefix[strlen(prefix) - 1] == '/' ? "" : "/");
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s%s%012llx"), prefix, delimiter, address);
    this->topic_prefix = std::string(temp_buffer);
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s/%s"), this->topic_prefix.c_str(), available_suffix);
    this->status_topic = std::string(temp_buffer);
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s/%s"), this->topic_prefix.c_str(), state_suffix);
    this->state_topic = std::string(temp_buffer);
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s/%s"), this->topic_prefix.c_str(), alert_state_suffix);
    this->alert_state_topic = std::string(temp_buffer);
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s/%s"), this->topic_prefix.c_str(), alert_set_suffix);
    this->alert_set_topic = std::string(temp_buffer);

    this->device_has_availability = !mqtt_client->get_availability().topic.empty() &&
                                    !mqtt_client->get_availability().payload_available.empty() &&
                                    !mqtt_client->get_availability().payload_not_available.empty();
    this->payload_online = mqtt_client->get_availability().payload_available.empty()
                               ? default_payload_online
                               : mqtt_client->get_availability().payload_available.c_str();
    this->payload_offline = mqtt_client->get_availability().payload_not_available.empty()
                                ? default_payload_offline
                                : mqtt_client->get_availability().payload_not_available.c_str();

    // ble-клиент
    this->ble_client_ = new myhomeiot_ble_client2::MyHomeIOT_BLEClient2();
    this->ble_client_->set_address(address);
    this->ble_client_->set_update_interval(update_interval);

    // сервисы ble-клиента
    myhomeiot_ble_client2::MyHomeIOT_BLEClientService *serv_bat = new myhomeiot_ble_client2::MyHomeIOT_BLEClientService();
    serv_bat->set_service_uuid16(0x180FULL);
    serv_bat->set_char_uuid16(0x2A19ULL);
    this->ble_client_->add_service(serv_bat);

    myhomeiot_ble_client2::MyHomeIOT_BLEClientService *serv_data = new myhomeiot_ble_client2::MyHomeIOT_BLEClientService();
    serv_data->set_service_uuid128(
        (uint8_t *)(const uint8_t[16]){0x1B, 0xC5, 0xD5, 0xA5, 0x02, 0x00, 0xB8, 0xAC, 0xE3, 0x11, 0xC7, 0xEA, 0x00, 0xB6, 0x4C, 0xC4});
    serv_data->set_char_uuid128(
        (uint8_t *)(const uint8_t[16]){0x1B, 0xC5, 0xD5, 0xA5, 0x02, 0x00, 0x8A, 0x91, 0xE3, 0x11, 0xCB, 0xEA, 0x20, 0x29, 0x48, 0x55});
    this->ble_client_->add_service(serv_data);

    myhomeiot_ble_client2::MyHomeIOT_BLEClientService *serv_alert = new myhomeiot_ble_client2::MyHomeIOT_BLEClientService();
    serv_alert->set_service_uuid16(0x1802ULL);
    serv_alert->set_char_uuid16(0x2A06ULL);
    serv_alert->set_skip_empty();
    serv_alert->set_value_template([=]() -> std::vector<uint8_t> {
      size_t index = this->alert_selected;
      size_t prev_value = this->alert_value;
      if (index >= 4) {
        if (index == 7 && prev_value == index)
          return {};
        this->alert_value = index;
        return {(uint8_t)(index == 7 ? 0x00 : index == 4 ? 0x01 : index == 5 ? 0x02 : 0x03)};
      } else {
        this->alert_value = 0;
        if (index > 0) {
          this->set_alert(0);
          return {(uint8_t)(index == 1 ? 0x01 : index == 2 ? 0x02 : 0x03)};
        } else if (prev_value == 7) {
          return {(uint8_t)0x02};
        } else {
          return {};
        }
      }
    });
    this->ble_client_->add_service(serv_alert);

    // автоматизации
    (new Automation<int, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
         new myhomeiot_ble_client2::MyHomeIOT_BLEClientConnectTrigger(this->ble_client_)))
        ->add_actions({new LambdaAction<int, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
            [=](int rssi, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &xthis) -> void {
              this->last_online = millis();
              this->online = true;
              this->rssi = rssi;
              this->next_time = 0;
            })});

    (new Automation<std::vector<uint8_t>, int, bool &, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
         new myhomeiot_ble_client2::MyHomeIOT_BLEClientValueTrigger(this->ble_client_)))
        ->add_actions({new LambdaAction<std::vector<uint8_t>, int, bool &, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
            [=](std::vector<uint8_t> x, int service, bool &stop_processing,
                const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &xthis) -> void {
              if (service == 1) {
                this->batt = x[0];
              } else if (service == 2) {
                this->temp = interpolate((float)(*(uint16_t *)&x[0]), temp_input, temp_output);
                this->humi = (float)(*(uint16_t *)&x[2]) / 13.0;
                float raw_soil = (float)(*(uint16_t *)&x[4]);
                this->soil = SOIL.unit == NULL ? raw_soil : interpolate(raw_soil, soil_input, soil_output, true);
                this->lumi = interpolate((float)(*(uint16_t *)&x[6]), lumi_input, lumi_output);
                this->new_state = true;
                this->next_time = 0;
              }
            })});

    (new Automation<uint32_t, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
         new myhomeiot_ble_client2::MyHomeIOT_BLEClientErrorTrigger(this->ble_client_)))
        ->add_actions({new LambdaAction<uint32_t, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
            [=](uint32_t error_count, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &xthis) -> void {
              this->error_count = error_count;
            })});

    set_component_source(id.c_str());
    ble_host->register_ble_client(this->ble_client_);
    App.register_component(this->ble_client_);
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Mclh09Device '%s'", id.c_str());
    ESP_LOGCONFIG(TAG, "  MAC address: %s", to_string(this->address_).c_str());
    ESP_LOGCONFIG(TAG, "  State Topic: '%s'", state_topic.c_str());
    ESP_LOGCONFIG(TAG, "  Status Topic: '%s'", status_topic.c_str());
    ESP_LOGCONFIG(TAG, "  Payload online/offline: '%s', '%s'", payload_online, payload_offline);
    ESP_LOGCONFIG(TAG, "  Alert State Topic: '%s'", alert_state_topic.c_str());
    ESP_LOGCONFIG(TAG, "  Alert Command Topic: '%s'", alert_set_topic.c_str());
  }

  void setup() override {
    ESP_LOGV(this->id.c_str(), "Setup");
    // use preferences to load/save selected alert
    this->pref_ = global_preferences->make_preference<size_t>(fnv1_hash(this->id), true);
    if (!this->pref_.load(&this->alert_selected) || this->alert_selected >= alert_options.size()) {
      ESP_LOGD(this->id.c_str(), "Unkwnown/new alert state pref: %d", this->alert_selected);
      this->alert_selected = 0;
    }
    this->new_alert = true;

    // subsribe for alert command topic
    this->mqtt_client_->subscribe(this->alert_set_topic, [=](const std::string &topic, const std::string &payload) {
      ESP_LOGD(this->id.c_str(), "Alert: '%s'", payload.c_str());
      for (size_t i = 0; i < alert_options.size(); i++)
        if (strncmp(alert_options[i].c_str(), payload.c_str(), alert_options[i].length()) == 0) {
          this->set_alert(i);
          this->ble_client_->force_update();
          return;
        }
      ESP_LOGE(this->id.c_str(), "Unkown alert '%s' received!", payload.c_str());
      this->set_alert(this->alert_selected);
    });
  }

  void loop() override {
    // idle until next time processing
    if (this->next_time > 0 && this->next_time > millis())
      return;

    // send discovery info
    bool discovery_enabled = this->mqtt_client_->is_discovery_enabled();
    if (this->connected && discovery_enabled && this->discovered_sensors < mclh09_sensors.size()) {
      if (discover_sensor(mclh09_sensors[this->discovered_sensors]))
        this->discovered_sensors++;
      return;
    }
    if (this->connected && discovery_enabled && this->discovered_selects < mclh09_selects.size()) {
      if (discover_select(mclh09_selects[this->discovered_selects]))
        this->discovered_selects++;
      return;
    }

    // consider device offline after 2 update interval since last online/connect
    if (this->online && this->last_online + 2 * this->ble_client_->get_update_interval() < millis()) {
      ESP_LOGD(this->id.c_str(), "Going to offline");
      this->online = false;
    }

    // online status changed
    if (this->connected && this->online != this->prev_online) {
      const char *payload = this->online ? this->payload_online : this->payload_offline;
      ESP_LOGD(this->id.c_str(), "Sending status '%s'", payload);
      if (!this->mqtt_client_->publish(this->status_topic, payload, strlen(payload), 0, true)) {
        ESP_LOGE(this->id.c_str(), "Failed to send status '%s' into topic '%s'", payload, this->status_topic.c_str());
      } else {
        this->prev_online = this->online;
      }
      return;
    }

    // sensor data updated
    if (this->connected && this->online && this->new_state) {
      snprintf_P(temp_buffer, sizeof(temp_buffer),
                 PSTR("{\"%s\":%d, \"%s\":%.1f, \"%s\":%.0f, \"%s\":%.0f, \"%s\":%.0f, \"%s\":%d, \"%s\":%d, \"time\":%u}"), BATT.id,
                 this->batt, TEMP.id, this->temp, LUMI.id, this->lumi, SOIL.id, this->soil, HUMI.id, this->humi, RSSI.id, this->rssi,
                 ERRORS.id, this->error_count, millis() / 1000);
      ESP_LOGD(this->id.c_str(), "Sending sensor data: '%s'", temp_buffer);
      if (!this->mqtt_client_->publish(this->state_topic, temp_buffer, strlen(temp_buffer))) {
        ESP_LOGE(this->id.c_str(), "Failed to send sensor data '%s' into topic '%s'", temp_buffer, this->state_topic.c_str());
      } else {
        this->new_state = false;
      }
      return;
    }
    // alert changed
    if (this->connected && this->online && this->new_alert) {
      std::string &selected = alert_options[this->alert_selected];
      ESP_LOGD(this->id.c_str(), "Sending alert state '%s'", selected.c_str());
      if (!this->mqtt_client_->publish(this->alert_state_topic, selected.c_str(), selected.length(), 0, true)) {
        ESP_LOGE(this->id.c_str(), "Failed to send alert state '%s' into topic '%s'", selected.c_str(), this->alert_state_topic.c_str());
      } else {
        this->new_alert = false;
      }
      return;
    }

    this->next_time = millis() + 10000;
    ESP_LOGV(this->id.c_str(), "Loop idle");
  }

  myhomeiot_ble_client2::MyHomeIOT_BLEClient2 *get_ble_client() { return this->ble_client_; }
  uint64_t get_address() { return this->address_; }
  void force_update() { this->ble_client_->force_update(); }
  void set_update_interval(uint32_t update_interval) { this->ble_client_->set_update_interval(update_interval); }

  void set_connected(bool connected) {
    if (!this->connected && connected) {
      this->discovered_sensors = this->discovered_selects = 0;
      this->new_alert = true;
    }
    this->next_time = 0;
    this->connected = connected;
  }

protected:
  std::string to_string(uint64_t address) const {
    char buffer[20];
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)(address >> 40), (uint8_t)(address >> 32), (uint8_t)(address >> 24),
            (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address >> 0));
    return std::string(buffer);
  }

private:
  mqtt::MQTTClientComponent *mqtt_client_;
  myhomeiot_ble_client2::MyHomeIOT_BLEClient2 *ble_client_;
  ESPPreferenceObject pref_;
  uint64_t address_;
  std::string id, name, topic_prefix, status_topic, state_topic, alert_state_topic, alert_set_topic;
  char temp_buffer[1024], temp_digit[8];
  bool device_has_availability;
  const char *payload_online, *payload_offline;
  bool connected = false, online = false, prev_online = false, new_state = false, new_alert = false;
  uint32_t last_online = 0, next_time = 0;
  size_t alert_selected, alert_value = 0, discovered_sensors = 0, discovered_selects = 0;
  int batt, rssi;
  float temp, lumi, soil, humi;
  uint32_t error_count = 0;

  void set_alert(size_t index) {
    this->alert_selected = index;
    this->pref_.save(&index);
    this->new_alert = true;
  }

  void base_config(const char *id, const char *name, const char *dev_class, const char *icon, const char *state_class, bool diag) {
    snprintf_P(temp_buffer, sizeof(temp_buffer),
               PSTR("{\"name\":\"%s\", \"uniq_id\":\"%s_%s\", \"~\":\"%s\", \"dev\":{\"ids\":[\"%s\"], \"name\":\"%s\", \"mdl\":\"Plant "
                    "sensor (MCLH-09)\", \"mf\":\"Life Control\"}"),
               name, this->id.c_str(), id, this->topic_prefix.c_str(), this->id.c_str(), this->name.c_str());

    if (this->device_has_availability) {
      strncat_P(temp_buffer, PSTR(", \"avty\":[{\"topic\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, this->mqtt_client_->get_availability().topic.c_str(), sizeof(temp_buffer) - strlen(temp_buffer));
      if (strncmp(payload_online, default_payload_online, strlen(default_payload_online)) != 0) {
        strncat_P(temp_buffer, PSTR("\", \"pl_avail\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
        strncat(temp_buffer, payload_online, sizeof(temp_buffer) - strlen(temp_buffer));
      }
      if (strncmp(payload_offline, default_payload_offline, strlen(default_payload_offline)) != 0) {
        strncat_P(temp_buffer, PSTR("\", \"pl_not_avail\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
        strncat(temp_buffer, payload_offline, sizeof(temp_buffer) - strlen(temp_buffer));
      }
      strncat_P(temp_buffer, PSTR("\"},{\"topic\": \"~/"), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, available_suffix, sizeof(temp_buffer) - strlen(temp_buffer));
      if (strncmp(payload_online, default_payload_online, strlen(default_payload_online)) != 0) {
        strncat_P(temp_buffer, PSTR("\", \"pl_avail\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
        strncat(temp_buffer, payload_online, sizeof(temp_buffer) - strlen(temp_buffer));
      }
      if (strncmp(payload_offline, default_payload_offline, strlen(default_payload_offline)) != 0) {
        strncat_P(temp_buffer, PSTR("\", \"pl_not_avail\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
        strncat(temp_buffer, payload_offline, sizeof(temp_buffer) - strlen(temp_buffer));
      }
      strncat_P(temp_buffer, PSTR("\"}], \"avty_mode\": \"all\""), sizeof(temp_buffer) - strlen(temp_buffer));

    } else {
      strncat_P(temp_buffer, PSTR(", \"avty_t\":\"~/"), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, available_suffix, sizeof(temp_buffer) - strlen(temp_buffer));
      if (strncmp(payload_online, default_payload_online, strlen(default_payload_online)) != 0) {
        strncat_P(temp_buffer, PSTR("\", \"pl_avail\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
        strncat(temp_buffer, payload_online, sizeof(temp_buffer) - strlen(temp_buffer));
      }
      if (strncmp(payload_offline, default_payload_offline, strlen(default_payload_offline)) != 0) {
        strncat_P(temp_buffer, PSTR("\", \"pl_not_avail\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
        strncat(temp_buffer, payload_offline, sizeof(temp_buffer) - strlen(temp_buffer));
      }
      strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }

    if (dev_class != NULL) {
      strncat_P(temp_buffer, PSTR(", \"dev_cla\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, dev_class, sizeof(temp_buffer) - strlen(temp_buffer));
      strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }
    if (icon != NULL) {
      strncat_P(temp_buffer, PSTR(", \"ic\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, icon, sizeof(temp_buffer) - strlen(temp_buffer));
      strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }
    if (state_class != NULL) {
      strncat_P(temp_buffer, PSTR(", \"stat_cla\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, state_class, sizeof(temp_buffer) - strlen(temp_buffer));
      strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }
    if (diag) {
      strncat_P(temp_buffer, PSTR(", \"ent_cat\":\"diagnostic\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }
  }

  bool discover_select(const Select *sel) {
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s/select/mclh09-%012llx/alert/config"),
               this->mqtt_client_->get_discovery_info().prefix.c_str(), this->address_);
    std::string config_topic{temp_buffer};
    ESP_LOGV(this->id.c_str(), "Config Topic (%d bytes): '%s'", config_topic.length(), config_topic.c_str());

    base_config(sel->id, sel->name, NULL, sel->icon, NULL, sel->diag);

    if (!sel->optimistic)
      strncat_P(temp_buffer, PSTR(", \"opt\":\"false\""), sizeof(temp_buffer) - strlen(temp_buffer));
    strncat_P(temp_buffer, PSTR(", \"stat_t\":\"~/"), sizeof(temp_buffer) - strlen(temp_buffer));
    strncat(temp_buffer, sel->state_suffix, sizeof(temp_buffer) - strlen(temp_buffer));
    strncat_P(temp_buffer, PSTR("\", \"cmd_t\":\"~/"), sizeof(temp_buffer) - strlen(temp_buffer));
    strncat(temp_buffer, sel->set_suffix, sizeof(temp_buffer) - strlen(temp_buffer));
    strncat_P(temp_buffer, PSTR("\", \"ops\":["), sizeof(temp_buffer) - strlen(temp_buffer));
    for (int i = 0; i < sel->options.size(); i++) {
      if (i == 0) {
        strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
      } else {
        strncat_P(temp_buffer, PSTR(",\""), sizeof(temp_buffer) - strlen(temp_buffer));
      }
      strncat(temp_buffer, sel->options[i].c_str(), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }
    strncat_P(temp_buffer, PSTR("]}"), sizeof(temp_buffer) - strlen(temp_buffer));

    ESP_LOGV(this->id.c_str(), "Config Data (%d bytes): '%s'", strlen(temp_buffer), temp_buffer);
    if (!this->mqtt_client_->publish(config_topic, temp_buffer, strlen(temp_buffer), 0, this->mqtt_client_->get_discovery_info().retain)) {
      ESP_LOGE(this->id.c_str(), "Failed to send discovery for select '%s' (%d bytes) into topic '%s'", sel->name, strlen(temp_buffer),
               config_topic.c_str());
      return false;
    }
    return true;
  }

  bool discover_sensor(const Sensor *sens) {
    snprintf_P(temp_buffer, sizeof(temp_buffer), PSTR("%s/sensor/mclh09-%012llx/%s/config"),
               this->mqtt_client_->get_discovery_info().prefix.c_str(), this->address_, sens->id);
    std::string config_topic{temp_buffer};
    ESP_LOGV(this->id.c_str(), "Config Topic (%d bytes): '%s'", config_topic.length(), config_topic.c_str());

    base_config(sens->id, sens->name, sens->dev_class, sens->icon, sens->state_class, sens->diag);

    if (sens->unit != NULL) {
      strncat_P(temp_buffer, PSTR(", \"unit_of_meas\":\""), sizeof(temp_buffer) - strlen(temp_buffer));
      strncat(temp_buffer, sens->unit, sizeof(temp_buffer) - strlen(temp_buffer));
      strncat_P(temp_buffer, PSTR("\""), sizeof(temp_buffer) - strlen(temp_buffer));
    }
    if (sens->accuracy > 0) {
      strncat_P(temp_buffer, PSTR(", \"sug_dsp_prc\":"), sizeof(temp_buffer) - strlen(temp_buffer));
      snprintf_P(temp_digit, sizeof(temp_digit), PSTR("%d"), sens->accuracy);
      strncpy(temp_buffer + strlen(temp_buffer), temp_digit, sizeof(temp_buffer) - strlen(temp_buffer));
    }

    strncat_P(temp_buffer, PSTR(", \"stat_t\":\"~/"), sizeof(temp_buffer) - strlen(temp_buffer));
    strncat(temp_buffer, state_suffix, sizeof(temp_buffer) - strlen(temp_buffer));
    strncat_P(temp_buffer, PSTR("\", \"val_tpl\":\"{{value_json."), sizeof(temp_buffer) - strlen(temp_buffer));
    strncat(temp_buffer, sens->id, sizeof(temp_buffer) - strlen(temp_buffer));
    strncat_P(temp_buffer, PSTR("}}\"}"), sizeof(temp_buffer) - strlen(temp_buffer));

    ESP_LOGV(this->id.c_str(), "Config Data (%d bytes): '%s'", strlen(temp_buffer), temp_buffer);
    if (!this->mqtt_client_->publish(config_topic, temp_buffer, strlen(temp_buffer), 0, this->mqtt_client_->get_discovery_info().retain)) {
      ESP_LOGE(this->id.c_str(), "Failed to send discovery for sensor '%s' (%d bytes) into topic '%s'", sens->name, strlen(temp_buffer),
               config_topic.c_str());
      return false;
    }
    return true;
  }

  float limit_value(float value, float min_value, float max_value) {
    return value < min_value ? min_value : value > max_value ? max_value : value;
  }

  float interpolate(float value, std::vector<float> &input, std::vector<float> &output, bool limit = false) {
    if (input.size() != output.size() || input.size() < 3) {
      ESP_LOGE(TAG, "Bad size of input/output arrays for interpolate()");
      return NAN;
    }
    size_t i = 0;
    while (i < input.size() - 1 && value < input[i + 1])
      i++;
    float result = (output[i] - output[i + 1]) * (value - input[i + 1]) / (input[i] - input[i + 1]) + output[i + 1];
    ESP_LOGV(TAG, "interpolate(%f) = %f, index = %d, (%f, %f)", value, result, i, input[i], input[i + 1]);
    return limit ? limit_value(result, output[output.size() - 1], output[0]) : result;
  }
};

class Mclh09MqttGateway : public Component {
public:
  Mclh09MqttGateway(mqtt::MQTTClientComponent *mqtt_client, myhomeiot_ble_host::MyHomeIOT_BLEHost *ble_host,
                    const std::vector<uint64_t> &mac_addresses, const std::string &topic_prefix, uint32_t update_interval = 3600000,
                    bool error_counting = false, bool raw_soil = false) {
    this->mqtt_client_ = mqtt_client;
    this->ble_host_ = ble_host;
    this->topic_prefix_ = topic_prefix;
    this->update_interval_ = update_interval;

    if (raw_soil)
      SOIL.unit = SOIL.dev_class = NULL;
    mclh09_sensors = {&BATT, &TEMP, &LUMI, &SOIL, &HUMI, &RSSI};
    if (error_counting)
mclh09_sensors.push_back(&ERRORS);

    (new Automation<>(new mqtt::MQTTConnectTrigger(this->mqtt_client_)))->add_actions({new LambdaAction<>([=]() -> void {
      ESP_LOGD(TAG, "Connected to MQTT");
      for (auto device : devices_)
        device->set_connected(true);
    })});

    (new Automation<>(new mqtt::MQTTDisconnectTrigger(this->mqtt_client_)))->add_actions({new LambdaAction<>([=]() -> void {
      ESP_LOGD(TAG, "Disconnected from MQTT");
      for (auto device : devices_)
        device->set_connected(false);
    })});

    for (auto address : mac_addresses)
      add_device(address);
  }

  void force_update() {
    for (auto device : devices_)
      device->force_update();
  }

  void parse_device(const esp32_ble_tracker::ESPBTDevice &x, std::string &packet) {
    if (packet.length() == 76 && strncasecmp(ADVR_MODEL, packet.c_str() + 60, strlen(ADVR_MODEL)) == 0) {
      uint64_t addr = x.address_uint64();
      for (auto device : devices_)
        if (device->get_address() == addr) {
          ESP_LOGD(TAG, "Parsed already found MCLH-09 '%s'", x.address_str().c_str());
          return;
        }
      ESP_LOGI(TAG, "Parsed new MCLH-09 '%s'", x.address_str().c_str());
      add_device(addr);
    }
  }

  void set_update_interval(uint32_t update_interval) {
    ESP_LOGD(TAG, "Setting update interval to %dms", update_interval);
    this->update_interval_ = update_interval;
    for (auto device : devices_)
      device->set_update_interval(update_interval);
  }

private:
  mqtt::MQTTClientComponent *mqtt_client_;
  myhomeiot_ble_host::MyHomeIOT_BLEHost *ble_host_;
  std::string topic_prefix_;
  std::vector<Mclh09Device *> devices_;
  uint32_t update_interval_;

  void add_device(uint64_t address) {
    Mclh09Device *new_device = new Mclh09Device(this->mqtt_client_, this->ble_host_, this->topic_prefix_, this->update_interval_, address);
    devices_.push_back(new_device);
    App.register_component(new_device);
  }
};

template <typename... Ts> class Mclh09MqttGatewayForceUpdateAction : public Action<Ts...> {
public:
  Mclh09MqttGatewayForceUpdateAction(Mclh09MqttGateway *parent) : parent_(parent) {}
  void play(Ts... x) override { parent_->force_update(); }

private:
  Mclh09MqttGateway *parent_;
};

} // namespace mclh_09_mqtt_gateway
} // namespace esphome

#endif
