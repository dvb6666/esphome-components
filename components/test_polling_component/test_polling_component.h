#pragma once
#ifndef TEST_POLLING_COMPONENT
#define TEST_POLLING_COMPONENT

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace test_polling_component {

static const char *const TAG = "test_polling_component";

class TestPollingComponent : public PollingComponent {
public:
  void setup() override {}
  void loop() override {}

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "TestPollingComponent");
    LOG_UPDATE_INTERVAL(this);
  }

  void update() override {
    this->counter_++;
    ESP_LOGD(TAG, "update(): counter=%d", this->counter_);
    this->on_update_callback_.call(this->counter_, *this);
  }

  void add_on_update_callback(std::function<void(uint32_t, const TestPollingComponent &)> &&callback) {
    this->on_update_callback_.add(std::move(callback));
  }

private:
  CallbackManager<void(uint32_t, const TestPollingComponent &)> on_update_callback_{};
  uint32_t counter_ = 0;
};

class TestPollingComponentUpdateTrigger : public Trigger<uint32_t, const TestPollingComponent &> {
public:
  explicit TestPollingComponentUpdateTrigger(TestPollingComponent *parent) {
    parent->add_on_update_callback([this](uint32_t counter, const TestPollingComponent &xthis) { this->trigger(counter, xthis); });
  }
};

} // namespace test_polling_component
} // namespace esphome

#endif
