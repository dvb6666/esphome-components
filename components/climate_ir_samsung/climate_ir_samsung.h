#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "ir_Samsung.h"

namespace esphome {
namespace climate_ir_samsung {

static const char *TAG = "climate_ir_samsung";

class SamsungAC : public Component, public climate::Climate {
 public:
  SamsungAC(InternalGPIOPin *pin, float default_temperature, float min_temperature, float max_temperature)
      : ir_pin_(pin),
        default_temperature_(default_temperature),
        min_temperature_(min_temperature),
        max_temperature_(max_temperature) {}

  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  void setup() override {
    if (this->sensor_) {
      this->sensor_->add_on_state_callback([this](float state) {
        this->current_temperature = state;
        this->publish_state();
      });
      this->current_temperature = this->sensor_->state;
    } else {
      this->current_temperature = NAN;
    }

    auto restore = this->restore_state_();
    if (restore.has_value()) {
      restore->apply(this);
    } else {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->target_temperature =
          roundf(clamp(this->current_temperature, this->min_temperature_, this->max_temperature_));
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }

    if (isnan(this->target_temperature)) {
      this->target_temperature = this->default_temperature_;
    }

    ac.begin();
    ac.on();
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      ac.off();
    } else if (this->mode == climate::CLIMATE_MODE_COOL) {
      ac.setMode(kSamsungAcCool);
    } else if (this->mode == climate::CLIMATE_MODE_DRY) {
      ac.setMode(kSamsungAcDry);
    } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
      ac.setMode(kSamsungAcFan);
    } else if (this->mode == climate::CLIMATE_MODE_HEAT) {
      ac.setMode(kSamsungAcHeat);
    } else if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
      ac.setMode(kSamsungAcFanAuto);
    }
    ac.setTemp(this->target_temperature);
    if (this->fan_mode == climate::CLIMATE_FAN_AUTO) {
      ac.setFan(kSamsungAcFanAuto);
    } else if (this->fan_mode == climate::CLIMATE_FAN_LOW) {
      ac.setFan(kSamsungAcFanLow);
    } else if (this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
      ac.setFan(kSamsungAcFanMed);
    } else if (this->fan_mode == climate::CLIMATE_FAN_HIGH) {
      ac.setFan(kSamsungAcFanHigh);
    }
    if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
      ac.setSwing(false);
    } else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
      ac.setSwing(true);
    }
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      ac.sendOff();
    } else {
      ac.send();
    }

    ESP_LOGD(TAG, "Samsung A/C remote is in the following state:");
    ESP_LOGD(TAG, "  %s\n", ac.toString().c_str());
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "IR Climate Samsung \"%s\"", this->get_name().c_str());
    LOG_PIN("  IR Pin: ", this->ir_pin_);
    ESP_LOGCONFIG(TAG, "  Default Temperature: %.1f", this->default_temperature_);
    ESP_LOGCONFIG(TAG, "  Min Temperature: %.1f", this->min_temperature_);
    ESP_LOGCONFIG(TAG, "  Max Temperature: %.1f", this->max_temperature_);
    ESP_LOGCONFIG(TAG, "  Temperature Sensor: %s", this->sensor_ ? this->sensor_->get_name().c_str() : "None");
  }

  climate::ClimateTraits traits() {
    auto traits = climate::ClimateTraits();
    traits.set_visual_min_temperature(this->min_temperature_);
    traits.set_visual_max_temperature(this->max_temperature_);
    traits.set_visual_temperature_step(1);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT_COOL,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_FAN_ONLY,
    });
    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
    });
    traits.set_supported_swing_modes({
        climate::CLIMATE_SWING_OFF,
        climate::CLIMATE_SWING_VERTICAL,
    });
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 1, 0)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
#else
    traits.set_supports_current_temperature(true);
#endif
    return traits;
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      climate::ClimateMode mode = *call.get_mode();
      if (mode == climate::CLIMATE_MODE_OFF) {
        ac.off();
      } else if (mode == climate::CLIMATE_MODE_COOL) {
        ac.on();
        ac.setMode(kSamsungAcCool);
      } else if (mode == climate::CLIMATE_MODE_DRY) {
        ac.on();
        ac.setMode(kSamsungAcDry);
      } else if (mode == climate::CLIMATE_MODE_FAN_ONLY) {
        ac.on();
        ac.setMode(kSamsungAcFan);
      } else if (mode == climate::CLIMATE_MODE_HEAT) {
        ac.on();
        ac.setMode(kSamsungAcHeat);
      } else if (mode == climate::CLIMATE_MODE_HEAT_COOL) {
        ac.on();
        ac.setMode(kSamsungAcFanAuto);
      }
      this->mode = mode;
    }

    if (call.get_target_temperature().has_value()) {
      float temp = *call.get_target_temperature();
      ac.setTemp(temp);
      this->target_temperature = temp;
    }

    if (call.get_fan_mode().has_value()) {
      climate::ClimateFanMode fan_mode = *call.get_fan_mode();
      if (fan_mode == climate::CLIMATE_FAN_AUTO) {
        ac.setFan(kSamsungAcFanAuto);
      } else if (fan_mode == climate::CLIMATE_FAN_LOW) {
        ac.setFan(kSamsungAcFanLow);
      } else if (fan_mode == climate::CLIMATE_FAN_MEDIUM) {
        ac.setFan(kSamsungAcFanMed);
      } else if (fan_mode == climate::CLIMATE_FAN_HIGH) {
        ac.setFan(kSamsungAcFanHigh);
      }
      this->fan_mode = fan_mode;
    }

    if (call.get_swing_mode().has_value()) {
      climate::ClimateSwingMode swing_mode = *call.get_swing_mode();
      if (swing_mode == climate::CLIMATE_SWING_OFF) {
        ac.setSwing(false);
      } else if (swing_mode == climate::CLIMATE_SWING_VERTICAL) {
        ac.setSwing(true);
      }
      this->swing_mode = swing_mode;
    }

    if (this->mode == climate::CLIMATE_MODE_OFF) {
      ac.sendOff();
    } else {
      ac.send();
    }

    this->publish_state();

    ESP_LOGD(TAG, "Samsung A/C remote is in the following state:");
    ESP_LOGD(TAG, "  %s\n", ac.toString().c_str());
  }

 private:
  InternalGPIOPin *ir_pin_;
  float default_temperature_, min_temperature_, max_temperature_;
  IRSamsungAc ac{ir_pin_->get_pin(), ir_pin_->is_inverted()};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace climate_ir_samsung
}  // namespace esphome
