#include "max_brightness_number.h"

#include "esphome/core/log.h"

namespace esphome::dfrobot_rgb_button {

static const char *const TAG = "dfrobot_rgb_button.number";

void MaxBrightnessNumber::setup() {
  if (this->restore_) {
    this->pref_ = global_preferences->make_preference<float>(this->restore_key_);
    float stored;
    if (this->pref_.load(&stored) && stored >= 0.0f && stored <= 100.0f)
      this->parent_->set_max_brightness(stored / 100.0f);
    // Persist every later change, no matter whether it comes from the frontend or an action.
    this->add_on_state_callback([this](float value) { this->pref_.save(&value); });
  }
  this->publish_state(this->parent_->get_max_brightness() * 100.0f);
}

void MaxBrightnessNumber::dump_config() {
  LOG_NUMBER("", "DFRobot RGB Button Max Brightness", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_));
}

void MaxBrightnessNumber::control(float value) {
  // The hub publishes the new state back to this entity.
  this->parent_->set_max_brightness(value / 100.0f);
}

}  // namespace esphome::dfrobot_rgb_button
