#include "stepper_cover.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stepper_cover_ {

using namespace esphome::cover;

static const char *const TAG = "stepper.cover";

StepperCover::StepperCover(stepper::Stepper *stepper) : stepper_(stepper) {
  this->traits_.set_is_assumed_state(false);
  this->traits_.set_supports_stop(true);
  this->traits_.set_supports_position(true);
  this->traits_.set_supports_tilt(false);
}

void StepperCover::setup() {
  std::string object_id("stepper_cover_" + this->get_object_id());
  uint32_t hash = fnv1_hash(object_id);
  ESP_LOGD(TAG, "Restoring data from preferences (object_id='%s', hash=0x%08x)", object_id.c_str(), hash);
  this->position_pref_ = global_preferences->make_preference<int32_t>(hash);
  int32_t restored_position = 0;
  if (!this->position_pref_.load(&restored_position)) {
    restored_position = 0;
    ESP_LOGD(TAG, "Position preference not loaded. First init?");
  } else {
    ESP_LOGD(TAG, "Restored stepper position: %d", restored_position);
    if (restored_position != 0) {
      this->stepper_->report_position(restored_position);
      this->stepper_->set_target((this->target_position_ = restored_position));
    }
  }
  this->current_operation = COVER_OPERATION_IDLE;
  this->update_position();
  // ESP_LOGCONFIG(TAG, "Setting up stepper cover '%s'. Init pos: %d", this->name_.c_str(), this->stepper_->current_position);
}

void StepperCover::dump_config() {
  // LOG_COVER("", "Stepper Cover", this);
  ESP_LOGCONFIG(TAG, "Stepper Cover '%s'", this->name_.c_str());
  ESP_LOGCONFIG(TAG, "  Max position: %d", this->max_position_);
  ESP_LOGCONFIG(TAG, "  Update delay: %dms", this->update_delay_);
}

void StepperCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    if (!this->moving_) {
      ESP_LOGD(TAG, "control(): received STOP but not moving");
      return;
    }
    ESP_LOGD(TAG, "control(): stop (current stepper pos %d)", this->stepper_->current_position);
    this->stepper_->set_target((this->target_position_ = this->stepper_->current_position));

  } else if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    this->target_position_ = (int32_t)round((COVER_OPEN - pos) * this->max_position_);
    // if (!this->inverted_)
    //   this->target_position_ = this->max_position_ - this->target_position_;
    this->current_operation = (this->position < pos ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING);
    this->last_position_ = this->stepper_->current_position;
    this->moving_ = true;

    ESP_LOGD(TAG, "control(): %s to %.2f%% (target stepper pos %d, current stepper pos %d)",
             (this->current_operation == COVER_OPERATION_OPENING ? "opening" : "closing"), pos, this->target_position_,
             this->stepper_->current_position);
    this->stepper_->set_target(this->target_position_);
    this->need_update_ = true;
  }
}

void StepperCover::loop() {
  if (this->moving_ && (this->stepper_->has_reached_target() ||
                        (this->next_update_ <= millis() && abs(this->last_position_ - this->stepper_->current_position) >= one_persent_))) {
    if (this->stepper_->has_reached_target()) {
      ESP_LOGD(TAG, "loop(): reached target %d (current %d)", this->target_position_, this->stepper_->current_position);
      this->moving_ = false;
      this->current_operation = COVER_OPERATION_IDLE;
      this->position_pref_.save(&this->stepper_->current_position);
    } else {
      this->next_update_ = millis() + this->update_delay_;
      this->last_position_ = this->stepper_->current_position;
    }

    // ESP_LOGD(TAG, "loop(): current stepper pos %d", this->stepper_->current_position);
    this->update_position();
  }

  else if (this->need_update_) {
    this->need_update_ = false;
    this->publish_state();
  }
}

CoverTraits StepperCover::get_traits() { return this->traits_; }

void StepperCover::set_speed(int speed) {
  ESP_LOGD(TAG, "set_speed(): %d steps/s", speed);
  this->stepper_->set_max_speed(speed);
}

void StepperCover::set_max_position(uint32_t max_position) {
  ESP_LOGD(TAG, "set_max_position(): %d steps", max_position);
  this->max_position_ = max_position;
  this->one_persent_ = max_position / 100;
}

void StepperCover::set_update_delay(uint32_t update_delay) {
  ESP_LOGD(TAG, "set_update_delay(): %d ms", update_delay);
  this->update_delay_ = update_delay;
}

void StepperCover::update_position() {
  this->position = clamp(COVER_OPEN - (float)this->stepper_->current_position / this->max_position_, 0.0f, 1.0f);
  this->need_update_ = true;
}
} // namespace stepper_cover_
} // namespace esphome
