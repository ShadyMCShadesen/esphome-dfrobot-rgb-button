#pragma once

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_state.h"

namespace esphome::dfrobot_rgb_button {

/// Smooth sine-shaped brightness "breathing" in the currently selected color (software effect).
class BreatheLightEffect : public light::LightEffect {
 public:
  explicit BreatheLightEffect(const char *name) : light::LightEffect(name) {}

  void start() override {
    this->started_at_ = millis();
    this->last_update_ = this->started_at_ - this->update_interval_;
  }

  void apply() override {
    const uint32_t now = millis();
    if (now - this->last_update_ < this->update_interval_)
      return;
    this->last_update_ = now;

    const float phase = static_cast<float>((now - this->started_at_) % this->period_) / this->period_;
    const float level = 0.5f - 0.5f * cosf(phase * 6.28318530718f);
    auto call = this->state_->turn_on();
    call.set_brightness(this->min_brightness_ + (this->max_brightness_ - this->min_brightness_) * level);
    call.set_transition_length(0);
    call.set_publish(false);
    call.set_save(false);
    call.perform();
  }

  void set_period(uint32_t period) { this->period_ = period; }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
  void set_min_max_brightness(float min, float max) {
    this->min_brightness_ = min;
    this->max_brightness_ = max;
  }

 protected:
  uint32_t period_{3000};
  uint32_t update_interval_{30};
  uint32_t started_at_{0};
  uint32_t last_update_{0};
  float min_brightness_{0.1f};
  float max_brightness_{1.0f};
};

/// Continuous color cycle using the formula of the vendor's "breatheLight" example:
/// R = |sin(t)|, G = |sin(t + 60 deg)|, B = |sin(t + 120 deg)| (software effect).
class ColorCycleLightEffect : public light::LightEffect {
 public:
  explicit ColorCycleLightEffect(const char *name) : light::LightEffect(name) {}

  void start() override {
    this->started_at_ = millis();
    this->last_update_ = this->started_at_ - this->update_interval_;
  }

  void apply() override {
    const uint32_t now = millis();
    if (now - this->last_update_ < this->update_interval_)
      return;
    this->last_update_ = now;

    // |sin| repeats every 180 degrees, so one period maps to pi.
    const float t = static_cast<float>((now - this->started_at_) % this->period_) / this->period_ * PI_F;
    auto call = this->state_->turn_on();
    call.set_rgb(fabsf(sinf(t)), fabsf(sinf(t + PI_F / 3.0f)), fabsf(sinf(t + 2.0f * PI_F / 3.0f)));
    call.set_transition_length(0);
    call.set_publish(false);
    call.set_save(false);
    call.perform();
  }

  void set_period(uint32_t period) { this->period_ = period; }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

 protected:
  static constexpr float PI_F = 3.14159265359f;

  uint32_t period_{18000};
  uint32_t update_interval_{50};
  uint32_t started_at_{0};
  uint32_t last_update_{0};
};

}  // namespace esphome::dfrobot_rgb_button
