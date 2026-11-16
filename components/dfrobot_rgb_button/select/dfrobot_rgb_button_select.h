#pragma once

#include <array>
#include <vector>

#include "esphome/core/helpers.h"
#include "esphome/components/select/select.h"
#include "../dfrobot_rgb_button.h"

namespace esphome::dfrobot_rgb_button {

/// Selects one of the hub's statuses; the option order matches the hub's status indices.
class StatusSelect : public select::Select, public Parented<DFRobotRGBButton> {
 protected:
  void control(size_t index) override;
};

/// Applies a named color to the module's light entity.
class PresetSelect : public select::Select, public Parented<DFRobotRGBButton> {
 public:
  void add_preset(float red, float green, float blue) { this->presets_.push_back({red, green, blue}); }

 protected:
  void control(size_t index) override;

  std::vector<std::array<float, 3>> presets_;
};

}  // namespace esphome::dfrobot_rgb_button
