#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/myhomeiot_ble_host/myhomeiot_ble_host.h"
#include "esphome/components/myhomeiot_ble_client2/myhomeiot_ble_client2.h"
#include "esphome/components/select/automation.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace mclh_09_gateway {

#define SENSOR_ID "mclh09_%03d_%s"
#define SENSOR_NAME "mclh09_%03d %s"
const char *TAG = "mclh-09";

class AlertSelect : public select::Select, public Component {

  void setup() override {
    set_disabled_by_default(false);
    set_icon("mdi:alarm-light");
    traits.set_options({"Off", "Red (once)", "Green (once)", "Red + green (once)", "Red (every update)", "Green (every update)",
                        "Red + green (every update)", "Green (always)"});
    this->pref_ = global_preferences->make_preference<size_t>(this->get_object_id_hash());
    size_t index = 0;
    if (!this->pref_.load(&index) || !this->has_index(index))
      index = 0;
    this->publish_state(this->at(index).value());
  }

  void control(const std::string &value) override {
    auto index = this->index_of(value);
    this->pref_.save(&index.value());
    this->publish_state(value);
  }

private:
  ESPPreferenceObject pref_;
};

class Mclh09Gateway : public Component {

public:
  Mclh09Gateway(const std::vector<uint64_t> &mac_addresses, uint32_t update_interval = 3600000, bool error_counting = false, bool raw_soil = false) {
    mac_addresses_ = mac_addresses;
    device_count = mac_addresses_.size();

    // выделяем динамические массивы сенсоров и ble-клиентов каждого вида
    batt_sensor = new sensor::Sensor *[device_count];
    temp_sensor = new sensor::Sensor *[device_count];
    lumi_sensor = new sensor::Sensor *[device_count];
    soil_sensor = new sensor::Sensor *[device_count];
    humi_sensor = new sensor::Sensor *[device_count];
    rssi_sensor = new sensor::Sensor *[device_count];
    alert_select = new AlertSelect *[device_count];
    alert_value = new size_t[device_count];
    ble_client = new myhomeiot_ble_client2::MyHomeIOT_BLEClient2 *[device_count];
    if (error_counting)
      error_sensor = new sensor::Sensor *[device_count];

    // создаём связи между ble-клиентами и сенсорами и регистрируем всё в Esphome
    char buffer[64];
    for (size_t i = 0; i < device_count; i++) {
      // датчик Battery Level
      batt_sensor[i] = new sensor::Sensor();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "batt");
      batt_sensor[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "batt");
      batt_sensor[i]->set_object_id(strdup(buffer));
      batt_sensor[i]->set_disabled_by_default(false);
      batt_sensor[i]->set_device_class("battery");
      batt_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
      batt_sensor[i]->set_unit_of_measurement("%");
      batt_sensor[i]->set_accuracy_decimals(0);
      batt_sensor[i]->set_force_update(false);
      App.register_sensor(batt_sensor[i]);

      // датчик температуры
      temp_sensor[i] = new sensor::Sensor();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "temp");
      temp_sensor[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "temp");
      temp_sensor[i]->set_object_id(strdup(buffer));
      temp_sensor[i]->set_disabled_by_default(false);
      temp_sensor[i]->set_device_class("temperature");
      temp_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
      temp_sensor[i]->set_unit_of_measurement("°C");
      temp_sensor[i]->set_accuracy_decimals(1);
      temp_sensor[i]->set_force_update(false);
      // temp_sensor[i]->set_filters({new sensor::CalibrateLinearFilter(0.09907442858106243f, -37.09368850461943f)});
      App.register_sensor(temp_sensor[i]);

      // датчик освещения
      lumi_sensor[i] = new sensor::Sensor();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "lumi");
      lumi_sensor[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "lumi");
      lumi_sensor[i]->set_object_id(strdup(buffer));
      lumi_sensor[i]->set_disabled_by_default(false);
      lumi_sensor[i]->set_device_class("illuminance");
      lumi_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
      lumi_sensor[i]->set_unit_of_measurement("lx");
      lumi_sensor[i]->set_accuracy_decimals(0);
      lumi_sensor[i]->set_force_update(false);
      // lumi_sensor[i]->set_filters({new sensor::CalibrateLinearFilter(96.48563036227942f, -13913.813751854166f),
      //                              new sensor::LambdaFilter([=](float x) -> optional<float> { return limit_value(x, 0, x); })});
      App.register_sensor(lumi_sensor[i]);

      // датчик влажности почвы
      soil_sensor[i] = new sensor::Sensor();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "soil");
      soil_sensor[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "soil");
      soil_sensor[i]->set_object_id(strdup(buffer));
      soil_sensor[i]->set_disabled_by_default(false);
      soil_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
      if (!raw_soil) {
        soil_sensor[i]->set_device_class("moisture");
        soil_sensor[i]->set_unit_of_measurement("%");
      }
      soil_sensor[i]->set_accuracy_decimals(0);
      soil_sensor[i]->set_force_update(false);
      // soil_sensor[i]->set_filters({new sensor::CalibrateLinearFilter(0.17831784356979544f, -165.0581022116415f),
      //                              new sensor::LambdaFilter([=](float x) -> optional<float> { return limit_value(x, 0, 60); })});
      App.register_sensor(soil_sensor[i]);

      // датчик влажности
      humi_sensor[i] = new sensor::Sensor();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "humi");
      humi_sensor[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "humi");
      humi_sensor[i]->set_object_id(strdup(buffer));
      humi_sensor[i]->set_disabled_by_default(false);
      humi_sensor[i]->set_device_class("humidity");
      humi_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
      humi_sensor[i]->set_unit_of_measurement("%");
      humi_sensor[i]->set_accuracy_decimals(0);
      humi_sensor[i]->set_force_update(false);
      // humi_sensor[i]->set_filters({new sensor::LambdaFilter([=](float x) -> optional<float> { return x / 13.0; })});
      App.register_sensor(humi_sensor[i]);

      // датчик уровня сигнала
      rssi_sensor[i] = new sensor::Sensor();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "rssi");
      rssi_sensor[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "rssi");
      rssi_sensor[i]->set_object_id(strdup(buffer));
      rssi_sensor[i]->set_disabled_by_default(false);
      rssi_sensor[i]->set_device_class("signal_strength");
      rssi_sensor[i]->set_icon("mdi:signal");
      rssi_sensor[i]->set_entity_category(ENTITY_CATEGORY_DIAGNOSTIC);
      rssi_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
      rssi_sensor[i]->set_unit_of_measurement("dBm");
      rssi_sensor[i]->set_accuracy_decimals(0);
      rssi_sensor[i]->set_force_update(false);
      App.register_sensor(rssi_sensor[i]);

      // датчик количества ошибок
      if (error_counting) {
        error_sensor[i] = new sensor::Sensor();
        snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "errors");
        error_sensor[i]->set_name(strdup(buffer));
        snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "errors");
        error_sensor[i]->set_object_id(strdup(buffer));
        error_sensor[i]->set_disabled_by_default(false);
        error_sensor[i]->set_icon("mdi:alert-circle");
        error_sensor[i]->set_entity_category(ENTITY_CATEGORY_DIAGNOSTIC);
        error_sensor[i]->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        error_sensor[i]->set_accuracy_decimals(0);
        error_sensor[i]->set_force_update(false);
        App.register_sensor(error_sensor[i]);
      }

      // alert select
      alert_value[i] = 0;
      alert_select[i] = new AlertSelect();
      snprintf(buffer, sizeof(buffer), SENSOR_NAME, i + 1, "alert select");
      alert_select[i]->set_name(strdup(buffer));
      snprintf(buffer, sizeof(buffer), SENSOR_ID, i + 1, "alert_select");
      alert_select[i]->set_object_id(strdup(buffer));
      App.register_component(alert_select[i]);
      App.register_select(alert_select[i]);

      // ble-клиент
      ble_client[i] = new myhomeiot_ble_client2::MyHomeIOT_BLEClient2();
      ble_client[i]->set_address(mac_addresses_[i]);
      ble_client[i]->set_update_interval(update_interval);
      App.register_component(ble_client[i]);

      // сервисы ble-клиента
      myhomeiot_ble_client2::MyHomeIOT_BLEClientService *serv_bat = new myhomeiot_ble_client2::MyHomeIOT_BLEClientService();
      serv_bat->set_service_uuid16(0x180FULL);
      serv_bat->set_char_uuid16(0x2A19ULL);
      ble_client[i]->add_service(serv_bat);

      myhomeiot_ble_client2::MyHomeIOT_BLEClientService *serv_data = new myhomeiot_ble_client2::MyHomeIOT_BLEClientService();
      serv_data->set_service_uuid128(
          (uint8_t *)(const uint8_t[16]){0x1B, 0xC5, 0xD5, 0xA5, 0x02, 0x00, 0xB8, 0xAC, 0xE3, 0x11, 0xC7, 0xEA, 0x00, 0xB6, 0x4C, 0xC4});
      serv_data->set_char_uuid128(
          (uint8_t *)(const uint8_t[16]){0x1B, 0xC5, 0xD5, 0xA5, 0x02, 0x00, 0x8A, 0x91, 0xE3, 0x11, 0xCB, 0xEA, 0x20, 0x29, 0x48, 0x55});
      ble_client[i]->add_service(serv_data);

      myhomeiot_ble_client2::MyHomeIOT_BLEClientService *serv_alert = new myhomeiot_ble_client2::MyHomeIOT_BLEClientService();
      serv_alert->set_service_uuid16(0x1802ULL);
      serv_alert->set_char_uuid16(0x2A06ULL);
      serv_alert->set_skip_empty();
      serv_alert->set_value_template([=]() -> std::vector<uint8_t> {
        size_t index = (alert_select[i]->active_index()).value_or(0);
        size_t prev_value = alert_value[i];
        if (index >= 4) {
          if (index == 7 && prev_value == index)
            return {};
          alert_value[i] = index;
          return {(uint8_t)(index == 7 ? 0x00 : index == 4 ? 0x01 : index == 5 ? 0x02 : 0x03)};
        } else {
          alert_value[i] = 0;
          if (index > 0) {
            auto call = alert_select[i]->make_call();
            call.set_index(0);
            call.perform();
            return {(uint8_t)(index == 1 ? 0x01 : index == 2 ? 0x02 : 0x03)};
          } else if (prev_value == 7)
            return {(uint8_t)0x02};
          else
            return {};
        }
      });
      ble_client[i]->add_service(serv_alert);

      // автоматизации
      (new Automation<std::string, size_t>(new select::SelectStateTrigger(alert_select[i])))
          ->add_actions({new LambdaAction<std::string, size_t>([=](std::string x, size_t sz) -> void { ble_client[i]->force_update(); })});

      (new Automation<int, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
           new myhomeiot_ble_client2::MyHomeIOT_BLEClientConnectTrigger(ble_client[i])))
          ->add_actions({new LambdaAction<int, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
              [=](int rssi, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &xthis) -> void {
                rssi_sensor[i]->publish_state(rssi);
              })});

      (new Automation<std::vector<uint8_t>, int, bool &, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
           new myhomeiot_ble_client2::MyHomeIOT_BLEClientValueTrigger(ble_client[i])))
          ->add_actions({new LambdaAction<std::vector<uint8_t>, int, bool &, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
              [=](std::vector<uint8_t> x, int service, bool &stop_processing,
                  const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &xthis) -> void {
                if (service == 1) {
                  batt_sensor[i]->publish_state(x[0]);
                } else if (service == 2) {
                  // temp_sensor[i]->publish_state((float)(*(uint16_t *)&x[0]));
                  // humi_sensor[i]->publish_state((float)(*(uint16_t *)&x[2]));
                  // soil_sensor[i]->publish_state((float)(*(uint16_t *)&x[4]));
                  // lumi_sensor[i]->publish_state((float)(*(uint16_t *)&x[6]));
                  temp_sensor[i]->publish_state(interpolate((float)(*(uint16_t *)&x[0]), temp_input, temp_output));
                  humi_sensor[i]->publish_state((float)(*(uint16_t *)&x[2]) / 13.0);
                  float soil = (float)(*(uint16_t *)&x[4]);
                  soil_sensor[i]->publish_state(raw_soil? soil : interpolate(soil, soil_input, soil_output, true));
                  lumi_sensor[i]->publish_state(interpolate((float)(*(uint16_t *)&x[6]), lumi_input, lumi_output));
                }
              })});

      if (error_counting) {
        (new Automation<uint32_t, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
             new myhomeiot_ble_client2::MyHomeIOT_BLEClientErrorTrigger(ble_client[i])))
            ->add_actions({new LambdaAction<uint32_t, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &>(
                [=](uint32_t error_count, const myhomeiot_ble_client2::MyHomeIOT_BLEClient2 &xthis) -> void {
                  error_sensor[i]->publish_state(error_count);
                })});
      }
    }
  }

  void set_ble_host(myhomeiot_ble_host::MyHomeIOT_BLEHost *blehost) {
    for (size_t i = 0; i < device_count; i++)
      blehost->register_ble_client(ble_client[i]);
  }

  void force_update() {
    for (size_t i = 0; i < device_count; i++)
      ble_client[i]->force_update();
  }

  void set_update_interval(uint32_t update_interval) {
    ESP_LOGD(TAG, "Setting update interval to %dms", update_interval);
    for (size_t i = 0; i < device_count; i++)
      ble_client[i]->set_update_interval(update_interval);
  }

protected:
  std::string to_string(uint64_t address) const {
    char buffer[20];
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)(address >> 40), (uint8_t)(address >> 32), (uint8_t)(address >> 24),
            (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address >> 0));
    return std::string(buffer);
  }

private:
  std::vector<uint64_t> mac_addresses_;
  size_t device_count;
  myhomeiot_ble_client2::MyHomeIOT_BLEClient2 **ble_client;
  sensor::Sensor **batt_sensor, **temp_sensor, **lumi_sensor, **soil_sensor, **humi_sensor, **rssi_sensor, **error_sensor;
  AlertSelect **alert_select;
  size_t *alert_value;
  std::vector<float> temp_input{1035, 909, 668, 424, 368, 273, 159, 0};
  std::vector<float> temp_output{68.8, 49.8, 24.3, 6.4, 1.0, -5.5, -20.5, -41.0};
  std::vector<float> soil_input{1280, 1254, 1249, 1202, 1104, 944, 900};
  std::vector<float> soil_output{66.0, 60.0, 58.0, 54.0, 22.0, 2.0, 0.0};
  std::vector<float> lumi_input{911, 764, 741, 706, 645, 545, 196, 117, 24, 17, 0};
  std::vector<float> lumi_output{175300.0, 45400.0, 32100.0, 20300.0, 14760.0, 7600.0, 1200.0, 444.0, 29.0, 17.0, 0.0};

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

template <typename... Ts> class Mclh09GatewayForceUpdateAction : public Action<Ts...> {
public:
  Mclh09GatewayForceUpdateAction(Mclh09Gateway *parent) : parent_(parent) {}
  void play(Ts... x) override { parent_->force_update(); }

private:
  Mclh09Gateway *parent_;
};

} // namespace mclh_09_gateway
} // namespace esphome

#endif
