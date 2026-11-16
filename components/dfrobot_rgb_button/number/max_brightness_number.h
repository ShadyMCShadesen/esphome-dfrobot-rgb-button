#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"
#include "../dfrobot_rgb_button.h"

namespace esphome::dfrobot_rgb_button {

/// Night mode: upper limit for the LED output in percent, applied to everything the module shows.
class MaxBrightnessNumber : public number::Number, public Component, public Parented<DFRobotRGBButton> {
 public:
  void setup() override;
  void dump_config() override;
  // Before the hub, so a restored limit is in place when the LED is written for the first time.
  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }

  void set_restore_key(uint32_t key) {
    this->restore_ = true;
    this->restore_key_ = key;
  }

 protected:
  void control(float value) override;

  ESPPreferenceObject pref_;
  uint32_t restore_key_{0};
  bool restore_{false};
};

}  // namespace esphome::dfrobot_rgb_button
