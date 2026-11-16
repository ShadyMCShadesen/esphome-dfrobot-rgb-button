#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/light/light_output.h"
#include "../dfrobot_rgb_button.h"

namespace esphome::dfrobot_rgb_button {

/// RGB light backed by the module's LED. The hub decides what is finally shown (status, flash, ...).
class DFRobotRGBButtonLight : public light::LightOutput, public Parented<DFRobotRGBButton> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void setup_state(light::LightState *state) override { this->parent_->set_light_state(state); }

  void write_state(light::LightState *state) override {
    float red, green, blue;
    // Brightness and gamma correction are already applied by the light core.
    state->current_values_as_rgb(&red, &green, &blue);
    this->parent_->set_light_color(red, green, blue);
  }
};

}  // namespace esphome::dfrobot_rgb_button
