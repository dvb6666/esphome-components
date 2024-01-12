#include "stepper_cover.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stepper_cover_ {

using namespace esphome::cover;

static const char *const TAG = "stepper.cover";

StepperCover::StepperCover(stepper::Stepper *stepper, bool restore_max_position, bool has_tilt_action, bool has_tilt_lambda, bool restore_tilt)
 : stepper_(stepper), restore_max_position_(restore_max_position) {
  this->traits_.set_is_assumed_state(false);
  this->traits_.set_supports_stop(true);
  this->traits_.set_supports_position(true);
  this->traits_.set_supports_tilt(has_tilt_action || has_tilt_lambda);
  this->tilt_trigger_ = has_tilt_action ? new Trigger<float>() : NULL;
  this->restore_tilt_ = restore_tilt && (has_tilt_action || has_tilt_lambda);
}

void StepperCover::setup() {
  if (this->restore_max_position_) {
    std::string object1_id("stepper_cover_p1_" + this->get_object_id());
    uint32_t hash1 = fnv1_hash(object1_id);
    ESP_LOGD(TAG, "Restoring max_position from preferences (object_id='%s', hash=0x%08x)", object1_id.c_str(), hash1);
    this->max_position_pref_ = global_preferences->make_preference<uint32_t>(hash1);
    uint32_t restored_max_position = 0;
    if (this->max_position_pref_.load(&restored_max_position)) {
      ESP_LOGD(TAG, "Restored max_position: %d", restored_max_position);
      this->set_max_position(restored_max_position, false);
    } else {
      ESP_LOGD(TAG, "Couldn't restore max_position");
    }
  }

  std::string object2_id("stepper_cover_p2_" + this->get_object_id());
  uint32_t hash2 = fnv1_hash(object2_id);
  ESP_LOGD(TAG, "Restoring position from preferences (object_id='%s', hash=0x%08x)", object2_id.c_str(), hash2);
  this->position_pref_ = global_preferences->make_preference<int32_t>(hash2);
  int32_t restored_position = 0;
  if (this->position_pref_.load(&restored_position)) {
    ESP_LOGD(TAG, "Restored position: %d", restored_position);
    if (restored_position != 0)
      this->reset_position(restored_position, false);
  } else {
    ESP_LOGD(TAG, "Couldn't restore position");
  }

  if (this->restore_tilt_) {
    std::string object3_id("stepper_cover_p3_" + this->get_object_id());
    uint32_t hash3 = fnv1_hash(object3_id);
    ESP_LOGD(TAG, "Restoring tilt from preferences (object_id='%s', hash=0x%08x)", object3_id.c_str(), hash3);
    this->tilt_pref_ = global_preferences->make_preference<float>(hash3);
    float restored_tilt = COVER_OPEN;
    if (this->tilt_pref_.load(&restored_tilt)) {
      ESP_LOGD(TAG, "Restored tilt: %f", restored_tilt);
      this->tilt = restored_tilt;
    } else {
      ESP_LOGD(TAG, "Couldn't restore tilt");
    }
  }
  this->current_operation = COVER_OPERATION_IDLE;
  this->update_position();
  this->init_ = true;
}

void StepperCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Stepper Cover '%s'", this->name_.c_str());
  ESP_LOGCONFIG(TAG, "  Max position: %d", this->max_position_);
  ESP_LOGCONFIG(TAG, "  Restore max position: %s", this->restore_max_position_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Restore tilt: %s", this->restore_tilt_ ? "true" : "false");
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

  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    this->target_position_ = (int32_t)round((COVER_OPEN - pos) * this->max_position_);
    this->current_operation = (this->position < pos ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING);
    this->last_position_ = this->stepper_->current_position;
    this->moving_ = true;

    ESP_LOGD(TAG, "control(): %s to %.2f%% (target stepper pos %d, current stepper pos %d)",
             (this->current_operation == COVER_OPERATION_OPENING ? "opening" : "closing"), pos, this->target_position_,
             this->stepper_->current_position);
    this->stepper_->set_target(this->target_position_);
    this->need_update_ = true;
  }
  if (call.get_tilt().has_value()) {
    auto tilt = *call.get_tilt();
    this->tilt_trigger_->trigger(tilt);
    this->update_tilt(tilt);
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
    return;
  }
  if (this->tilt_f_.has_value()) {
    auto s = (*this->tilt_f_)();
    if (s.has_value()) {
      auto tilt = clamp(*s, 0.0f, 1.0f);
      if (tilt != this->tilt) {
        ESP_LOGD(TAG, "loop(): tilt changed from %f to %f", this->tilt, tilt);
        this->update_tilt(tilt);
        return;
      }
    }
  }
  if (this->need_update_) {
    this->need_update_ = false;
    this->publish_state();
  }
}

CoverTraits StepperCover::get_traits() { return this->traits_; }

void StepperCover::set_speed(int speed) {
  ESP_LOGD(TAG, "set_speed(): %d steps/s", speed);
  this->stepper_->set_max_speed(speed);
}

void StepperCover::reset_position(int32_t position, bool save) {
  ESP_LOGD(TAG, "set_position(): %d steps", position);
  this->stepper_->report_position(position);
  this->stepper_->set_target((this->target_position_ = position));
  if (save)
    this->position_pref_.save(&position);
}

void StepperCover::set_max_position(uint32_t max_position, bool save) {
  ESP_LOGD(TAG, "set_max_position(): %d steps", max_position);
  this->max_position_ = max_position;
  this->one_persent_ = max_position / 100;
  if (save && this->restore_max_position_ && this->init_)
    this->max_position_pref_.save(&max_position);
}

void StepperCover::set_update_delay(uint32_t update_delay) {
  ESP_LOGD(TAG, "set_update_delay(): %d ms", update_delay);
  this->update_delay_ = update_delay;
}

void StepperCover::update_position() {
  this->position = clamp(COVER_OPEN - (float)this->stepper_->current_position / this->max_position_, 0.0f, 1.0f);
  this->need_update_ = true;
}

void StepperCover::update_tilt(float tilt) {
  this->tilt = tilt;
  this->need_update_ = true;
  if (this->restore_tilt_)
    this->tilt_pref_.save(&this->tilt);
}
} // namespace stepper_cover_
} // namespace esphome
