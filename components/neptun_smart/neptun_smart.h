#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/modbus_tcp/modbus_tcp.h"

#include <vector>

// #define MAX_WIRE_LINE_LEAK_SENSORS 4

namespace esphome {
namespace neptun_smart {

static const char *TAG = "neptun-smart";

#define MAX_ZONES 2
#define MAX_WIRE_LINE_LEAK_SENSORS 4
#define MAX_WATER_COUNTERS 8

#define READ_CONFIG_CODE 1
#define READ_WIRELESS_COUNT_CODE 2
#define READ_WATER_COUNTERS_CODE 3
#define SET_REGISTER_0 4

#define GET_BIT(number, bit) ((number & (1 << bit)) > 0 ? true : false)
#define SET_BIT(number, bit, value) (value ? (number | (1 << bit)) : (number & ~(1 << bit)))

class NeptunSmartComponent : public PollingComponent {
 public:
  NeptunSmartComponent(modbus_tcp::ModbusTcpComponent *modbus_tcp) : modbus_tcp_(modbus_tcp) {
    uint8_t i;
    for (i = 0; i < MAX_ZONES; i++) {
      alert_zone_sensor_[i] = nullptr;
      valve_zone_switch_[i] = nullptr;
    }
    for (i = 0; i < MAX_WIRE_LINE_LEAK_SENSORS; i++)
      wire_line_leak_sensor_[i] = nullptr;
    for (i = 0; i < MAX_WATER_COUNTERS; i++)
      water_counter_sensor_[i] = nullptr;
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; };

  void setup() override {
    // set Neptun Smart default MODBUS-address, if it was not set on ModbusTcpComponent
    if (modbus_tcp_->get_address() == 0)
      modbus_tcp_->set_address(240);
    // on connect request all configs and statuses
    modbus_tcp_->set_on_connect([this]() {
      next_update_time_ = 1;
      if (connection_status_sensor_)
        connection_status_sensor_->publish_state(true);
    });
    // on disconnect pause regular updating
    modbus_tcp_->set_on_disconnect([this]() {
      can_write_ = false;
      next_update_time_ = 0;
      if (connection_status_sensor_)
        connection_status_sensor_->publish_state(false);
    });
    // process received data on register response
    modbus_tcp_->set_on_register_response([this](uint8_t code, uint8_t *data, uint16_t register_count) {
      ESP_LOGV(TAG, "Received %d bytes for command 0x%02x", register_count, code);
      switch (code) {
        case READ_CONFIG_CODE: {
          if (register_count != 4) {
            ESP_LOGW(TAG, "Wrong registers count (%d) for config and status response", register_count);
          } else {
            for (uint8_t i = 0; i < 4; i++)
              last_config_and_status_[i] = (uint16_t) data[2 * i] << 8 | data[2 * i + 1];
            state_config_and_status_ = 1;
            can_write_ = true;
          }
        } break;

        case READ_WIRELESS_COUNT_CODE: {
          if (register_count != 1) {
            ESP_LOGW(TAG, "Wrong registers count (%d) for wireless count response", register_count);
          } else if (wireless_sensors_count_sensor_) {
            float new_state = (float) ((uint16_t) data[0] << 8 | data[1]);
            ESP_LOGV(TAG, "Got READ_WIRELESS_COUNT_CODE response 0x%04x", new_state);
            if (!wireless_sensors_count_sensor_->has_state() ||
                new_state != wireless_sensors_count_sensor_->get_state())
              wireless_sensors_count_sensor_->publish_state(new_state);
          }
        } break;

        case READ_WATER_COUNTERS_CODE: {
          if (register_count != 16) {
            ESP_LOGW(TAG, "Wrong registers count (%d) for water counters response", register_count);
          } else {
            for (uint8_t i = 0; i < MAX_WATER_COUNTERS; i++)
              last_water_counters_[i] = (uint32_t) data[4 * i] << 24 | (uint32_t) data[4 * i + 1] << 16 |
                                        (uint32_t) data[4 * i + 2] << 8 | data[4 * i + 3];
            state_water_counters_ = 1;
          }
        } break;

        case SET_REGISTER_0: {
          if (register_count != 1) {
            ESP_LOGW(TAG, "Wrong registers count (%d) for write response with code %d", register_count, code);
          } else {
            last_config_and_status_[0] = (uint16_t) data[0] << 8 | data[1];
            ESP_LOGV(TAG, "Got SET_REGISTER_0 response 0x%04x", last_config_and_status_[0]);
            state_config_and_status_ = 1;
          }
        } break;

        default:
          ESP_LOGD(TAG, "Got %d bytes for unknown response code 0x%02x", register_count, code);
      }
    });
  };

  void loop() override {
    unsigned long current_time;
    if (next_update_time_ > 0 && next_update_time_ < (current_time = millis())) {
      ESP_LOGV(TAG, "Update information");
      next_update_time_ = current_time + get_update_interval();
      read_config_and_status();
      if (wireless_sensors_count_sensor_)
        read_wireless_sensors_count();
      if (has_water_counters_)
        read_water_counters();
      return;
    }
    if (state_config_and_status_ > 0) {
      uint16_t bit = (state_config_and_status_++) - 1;
      if (bit > 32)
        state_config_and_status_ = 0;
      else if (bit == 0 && floor_washing_mode_switch_)
        floor_washing_mode_switch_->publish_state(GET_BIT(last_config_and_status_[0], 0));
      else if (bit >= 1 && bit < 1 + MAX_ZONES && alert_zone_sensor_[bit - 1])
        alert_zone_sensor_[bit - 1]->publish_state(GET_BIT(last_config_and_status_[0], bit));
      else if (bit == 3 && wireless_sensor_discharged_sensor_)
        wireless_sensor_discharged_sensor_->publish_state(GET_BIT(last_config_and_status_[0], 3));
      else if (bit == 4 && wireless_sensor_lost_sensor_)
        wireless_sensor_lost_sensor_->publish_state(GET_BIT(last_config_and_status_[0], 4));
      else if (bit == 7 && add_wireless_mode_switch_)
        add_wireless_mode_switch_->publish_state(GET_BIT(last_config_and_status_[0], 7));
      else if (bit >= 8 && bit < 8 + MAX_ZONES && valve_zone_switch_[bit - 8])
        valve_zone_switch_[bit - 8]->publish_state(GET_BIT(last_config_and_status_[0], bit));
      else if (bit == 10 && dual_zone_mode_switch_) {
        bool state = GET_BIT(last_config_and_status_[0], 10);
        dual_zone_mode_switch_->publish_state(state);
        // show/hide zone2 controls; need esp RESTART after changing
        bool internal = !state;
        if (alert_zone_sensor_[1])
          alert_zone_sensor_[1]->set_internal(internal);
        if (valve_zone_switch_[1])
          valve_zone_switch_[1]->set_internal(internal);
      } else if (bit == 11 && close_on_wireless_lost_switch_)
        close_on_wireless_lost_switch_->publish_state(GET_BIT(last_config_and_status_[0], 11));
      else if (bit == 12 && child_lock_switch_)
        child_lock_switch_->publish_state(GET_BIT(last_config_and_status_[0], 12));
      else if (bit >= 24 && bit < 24 + MAX_WIRE_LINE_LEAK_SENSORS && wire_line_leak_sensor_[bit - 24]) {
        wire_line_leak_sensor_[bit - 24]->publish_state(GET_BIT(last_config_and_status_[3], (bit - 24)));
      }
      return;
    }
    if (state_water_counters_ > 0) {
      uint16_t idx = (state_water_counters_++) - 1;
      if (idx > MAX_WATER_COUNTERS)
        state_water_counters_ = 0;
      else if (idx < MAX_WATER_COUNTERS && water_counter_sensor_[idx]) {
        float new_state = (float) last_water_counters_[idx] / 1000.0;
        if (!water_counter_sensor_[idx]->has_state() || new_state != water_counter_sensor_[idx]->get_state())
          water_counter_sensor_[idx]->publish_state(new_state);
      }
      return;
    }
  };

  void update() override {};

  void dump_config() override {
    uint8_t i;
    ESP_LOGCONFIG(TAG, "Neptun Smart");
    if (connection_status_sensor_)
      LOG_BINARY_SENSOR("  ", "Connection Status Sensor", connection_status_sensor_);
    if (dual_zone_mode_switch_)
      LOG_SWITCH("  ", "Dual Zone Mode Switch:", dual_zone_mode_switch_);
    for (i = 0; i < MAX_ZONES; i++)
      if (alert_zone_sensor_[i])
        LOG_BINARY_SENSOR("  ", "Alert Zone Sensor", alert_zone_sensor_[i]);
    for (i = 0; i < MAX_ZONES; i++)
      if (valve_zone_switch_[i])
        LOG_SWITCH("  ", "Valve Zone Switch", valve_zone_switch_[i]);
    if (floor_washing_mode_switch_)
      LOG_SWITCH("  ", "Floor Washing Mode Switch:", floor_washing_mode_switch_);
    if (add_wireless_mode_switch_)
      LOG_SWITCH("  ", "Adding Wireless Sensor Switch:", add_wireless_mode_switch_);
    if (close_on_wireless_lost_switch_)
      LOG_SWITCH("  ", "Close On Wireless Sensor Lost Switch:", close_on_wireless_lost_switch_);
    if (child_lock_switch_)
      LOG_SWITCH("  ", "Child Lock Switch:", child_lock_switch_);
    if (wireless_sensor_discharged_sensor_)
      LOG_BINARY_SENSOR("  ", "Wireless Sensor Discharged Sensor", wireless_sensor_discharged_sensor_);
    if (wireless_sensor_lost_sensor_)
      LOG_BINARY_SENSOR("  ", "Wireless Sensor Lost Sensor", wireless_sensor_lost_sensor_);
    if (wireless_sensors_count_sensor_)
      LOG_SENSOR("  ", "Wireless Sensors Count Sensor:", wireless_sensors_count_sensor_);
    for (i = 0; i < MAX_WIRE_LINE_LEAK_SENSORS; i++)
      if (wire_line_leak_sensor_[i])
        LOG_BINARY_SENSOR("  ", "Wire Line Leak Sensor", wire_line_leak_sensor_[i]);
    for (i = 0; i < MAX_WATER_COUNTERS; i++)
      if (water_counter_sensor_[i])
        LOG_SENSOR("  ", "Water Counter Sensor", water_counter_sensor_[i]);
  };

  bool can_write() { return can_write_; }

  void set_connection_status_sensor(binary_sensor::BinarySensor *sensor) { connection_status_sensor_ = sensor; }
  void set_dual_zone_mode_switch(switch_::Switch *swt) { dual_zone_mode_switch_ = swt; }
  void set_alert_zone_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
    if (index < MAX_ZONES)
      alert_zone_sensor_[index] = sensor;
  }
  void set_valve_zone_switch(uint8_t index, switch_::Switch *swt) {
    if (index < MAX_ZONES)
      valve_zone_switch_[index] = swt;
  }
  void set_wireless_sensor_lost_sensor(binary_sensor::BinarySensor *sensor) { wireless_sensor_lost_sensor_ = sensor; }
  void set_floor_washing_mode_switch(switch_::Switch *swt) { floor_washing_mode_switch_ = swt; }
  void set_add_wireless_mode_switch(switch_::Switch *swt) { add_wireless_mode_switch_ = swt; }
  void set_close_on_wireless_lost_switch(switch_::Switch *swt) { close_on_wireless_lost_switch_ = swt; }
  void set_child_lock_switch(switch_::Switch *swt) { child_lock_switch_ = swt; }
  void set_wireless_sensor_discharged_sensor(binary_sensor::BinarySensor *sensor) {
    wireless_sensor_discharged_sensor_ = sensor;
  }
  void set_wireless_sensors_count_sensor(sensor::Sensor *sensor) { wireless_sensors_count_sensor_ = sensor; }

  void set_wire_line_leak_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
    if (index < MAX_WIRE_LINE_LEAK_SENSORS) {
      wire_line_leak_sensor_[index] = sensor;
      has_wire_line_leak_sensor_ = true;
    }
  }

  void set_water_counter_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < MAX_WATER_COUNTERS) {
      water_counter_sensor_[index] = sensor;
      has_water_counters_ = true;
    }
  }

  void read_config_and_status() { modbus_tcp_->read_registers(READ_CONFIG_CODE, 0, 4); }
  void read_wireless_sensors_count() { modbus_tcp_->read_registers(READ_WIRELESS_COUNT_CODE, 6, 1); }
  void read_water_counters() { modbus_tcp_->read_registers(READ_WATER_COUNTERS_CODE, 107, 16); }

  void set_floor_washing_mode(bool state) {
    ESP_LOGD(TAG, "Setting floor washing mode to: %s", state ? "on" : "off");
    modbus_tcp_->write_register(SET_REGISTER_0, 0, SET_BIT(last_config_and_status_[0], 0, state));
  }

  void set_add_wireless_mode(bool state) {
    ESP_LOGD(TAG, "Setting add wireless sensor mode to: %s", state ? "on" : "off");
    modbus_tcp_->write_register(SET_REGISTER_0, 0, SET_BIT(last_config_and_status_[0], 7, state));
  }

  void set_valve_zone(uint8_t index, bool state) {
    if (index < MAX_ZONES) {
      ESP_LOGD(TAG, "Setting valve zone %d to: %s", index + 1, state ? "on" : "off");
      // for dual-zone mode change only one valve bit (8 or 9); for non-dual change both valve bits (8 and 9)
      uint16_t new_value = GET_BIT(last_config_and_status_[0], 10)
                               ? SET_BIT(last_config_and_status_[0], (8 + index), state)
                               : SET_BIT(SET_BIT(last_config_and_status_[0], 8, state), 9, state);
      modbus_tcp_->write_register(SET_REGISTER_0, 0, new_value);
    }
  }

  void set_dual_zone_mode(bool state) {
    ESP_LOGD(TAG, "Setting dual zone mode to: %s", state ? "on" : "off");
    modbus_tcp_->write_register(SET_REGISTER_0, 0, SET_BIT(last_config_and_status_[0], 10, state));
  }

  void set_close_on_wireless_lost(bool state) {
    ESP_LOGD(TAG, "Setting close on wireless sensor lost to: %s", state ? "on" : "off");
    modbus_tcp_->write_register(SET_REGISTER_0, 0, SET_BIT(last_config_and_status_[0], 11, state));
  }

  void set_child_lock(bool state) {
    ESP_LOGD(TAG, "Setting child lock to: %s", state ? "on" : "off");
    modbus_tcp_->write_register(SET_REGISTER_0, 0, SET_BIT(last_config_and_status_[0], 12, state));
  }

 protected:
  modbus_tcp::ModbusTcpComponent *modbus_tcp_;
  uint16_t state_config_and_status_{0}, state_water_counters_{0};
  uint16_t last_config_and_status_[4]{0, 0, 0, 0};
  uint32_t last_water_counters_[MAX_WATER_COUNTERS];
  binary_sensor::BinarySensor *alert_zone_sensor_[MAX_ZONES];
  switch_::Switch *valve_zone_switch_[MAX_ZONES];
  switch_::Switch *zone_1_switch_{nullptr}, *zone_2_switch_{nullptr}, *dual_zone_mode_switch_{nullptr},
      *floor_washing_mode_switch_{nullptr}, *add_wireless_mode_switch_{nullptr},
      *close_on_wireless_lost_switch_{nullptr}, *child_lock_switch_{nullptr};
  binary_sensor::BinarySensor *connection_status_sensor_{nullptr}, *wireless_sensor_discharged_sensor_{nullptr},
      *wireless_sensor_lost_sensor_{nullptr};
  sensor::Sensor *wireless_sensors_count_sensor_{nullptr};
  binary_sensor::BinarySensor *wire_line_leak_sensor_[MAX_WIRE_LINE_LEAK_SENSORS];
  sensor::Sensor *water_counter_sensor_[MAX_WATER_COUNTERS];
  bool has_wire_line_leak_sensor_{false}, has_water_counters_{false}, can_write_{false};
  unsigned long next_update_time_{0};
};

class AbstractApplySwitch : public switch_::Switch, public Parented<NeptunSmartComponent> {
 protected:
  virtual void apply_state(bool state) = 0;
  void write_state(bool state) override {
    if (parent_->can_write()) {
      publish_state(state);
      apply_state(state);
    } else {
      ESP_LOGW(TAG, "Device not connected or config was not received yet!");
    }
  }
};

class ValveZoneSwitch : public AbstractApplySwitch {
  void apply_state(bool state) override { parent_->set_valve_zone(index_, state); }
  uint8_t index_;

 public:
  void set_index(uint8_t index) { index_ = index; }
};

class DualZoneModeSwitch : public AbstractApplySwitch {
  void apply_state(bool state) override { parent_->set_dual_zone_mode(state); }
};

class FloorModeSwitch : public AbstractApplySwitch {
  void apply_state(bool state) override { parent_->set_floor_washing_mode(state); }
};

class AddWirelessModeSwitch : public AbstractApplySwitch {
  void apply_state(bool state) override { parent_->set_add_wireless_mode(state); }
};

class CloseOnWirelessLostSwitch : public AbstractApplySwitch {
  void apply_state(bool state) override { parent_->set_close_on_wireless_lost(state); }
};

class ChildLockSwitch : public AbstractApplySwitch {
  void apply_state(bool state) override { parent_->set_child_lock(state); }
};

}  // namespace neptun_smart
}  // namespace esphome
