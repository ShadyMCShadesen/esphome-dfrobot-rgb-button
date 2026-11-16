#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"
#include "../dfrobot_rgb_button.h"

namespace esphome::dfrobot_rgb_button {

/// Child lock: while on, presses are ignored by the hub (no events, no toggle, no statistics).
class LockSwitch : public switch_::Switch, public Component, public Parented<DFRobotRGBButton> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::dfrobot_rgb_button
