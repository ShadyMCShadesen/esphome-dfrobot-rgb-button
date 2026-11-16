#include "dfrobot_rgb_button_select.h"

namespace esphome::dfrobot_rgb_button {

void StatusSelect::control(size_t index) {
  // The hub publishes the new state back to this entity.
  this->parent_->set_status(static_cast<uint8_t>(index));
}

void PresetSelect::control(size_t index) {
  if (index >= this->presets_.size())
    return;
  const auto &preset = this->presets_[index];
  this->parent_->apply_light_color(preset[0], preset[1], preset[2]);
  this->publish_state(index);
}

}  // namespace esphome::dfrobot_rgb_button
