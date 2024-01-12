#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/stepper/stepper.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace stepper_cover_ {

class StepperCover : public cover::Cover, public Component {
public:
  StepperCover(stepper::Stepper *stepper, bool restore_max_position, bool has_tilt_action, bool has_tilt_lambda, bool restore_tilt);

  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_speed(int speed);
  void reset_position(int32_t position, bool save = true);
  void set_max_position(uint32_t max_position, bool save = true);
  void set_update_delay(uint32_t update_delay);
  Trigger<float> *get_tilt_trigger() const { return this->tilt_trigger_; }
  void set_tilt_lambda(std::function<optional<float>()> &&tilt_f) { this->tilt_f_ = tilt_f; }

protected:
  void control(const cover::CoverCall &call) override;
  cover::CoverTraits get_traits() override;
  void update_position();
  void update_tilt(float tilt);

private:
  stepper::Stepper *stepper_{nullptr};
  bool restore_max_position_, restore_tilt_;
  uint32_t max_position_{1000}, one_persent_{10}, update_delay_{500};
  bool init_{false}, moving_{false}, need_update_{false};
  int32_t target_position_{0}, last_position_{0};
  unsigned long next_update_{0};
  ESPPreferenceObject position_pref_{nullptr}, max_position_pref_{nullptr}, tilt_pref_{nullptr};
  cover::CoverTraits traits_{};
  Trigger<float> *tilt_trigger_;
  optional<std::function<optional<float>()>> tilt_f_;
};

} // namespace stepper_cover_
} // namespace esphome