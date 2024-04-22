#pragma once
#ifndef GPIO_VIEWER_PROXY
#define GPIO_VIEWER_PROXY

#ifdef USE_ESP32

#include "esphome/core/base_automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

#include "gpio_viewer.h"

namespace esphome {
namespace gpio_viewer {

static const char *const TAG = "gpio_viewer_proxy";

class GPIOViewerProxy : public Component {
public:
  GPIOViewerProxy(wifi::WiFiComponent *wifi_wificomponent, uint16_t port, uint32_t sampling_interval) {
    this->port = port;
    this->sampling_interval = sampling_interval;
    this->gpio_viewer = new GPIOViewer();

    auto automation = new Automation<>(wifi_wificomponent->get_connect_trigger());
    automation->add_actions({new LambdaAction<>([=]() -> void { this->start(); })});
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "GPIO Viewer Proxy");
    ESP_LOGCONFIG(TAG, "  Port: %d", this->port);
    ESP_LOGCONFIG(TAG, "  Sampling Interval: %d", this->sampling_interval);
  }

  void setup() override {}

  void loop() override {}

protected:
  void start() {
    if (!this->started) {
      this->started = true;
      ESP_LOGD(TAG, "start!");
      this->gpio_viewer->setPort(this->port);
      this->gpio_viewer->setSamplingInterval(this->sampling_interval);
      this->gpio_viewer->begin();
    }
  }

private:
  GPIOViewer *gpio_viewer;
  uint16_t port;
  uint32_t sampling_interval;
  bool started = false;
};

} // namespace gpio_viewer
} // namespace esphome

#endif
#endif
