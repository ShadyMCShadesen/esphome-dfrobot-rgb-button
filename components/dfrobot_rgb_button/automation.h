#pragma once

#include <string>

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "dfrobot_rgb_button.h"

namespace esphome::dfrobot_rgb_button {

template<typename... Ts>
class FlashAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  TEMPLATABLE_VALUE(float, red)
  TEMPLATABLE_VALUE(float, green)
  TEMPLATABLE_VALUE(float, blue)
  TEMPLATABLE_VALUE(uint32_t, count)
  TEMPLATABLE_VALUE(uint32_t, duration)
  TEMPLATABLE_VALUE(uint32_t, pause)

  void play(const Ts &...x) override {
    this->parent_->flash(this->red_.value(x...), this->green_.value(x...), this->blue_.value(x...),
                         this->count_.value(x...), this->duration_.value(x...), this->pause_.value(x...));
  }
};

template<typename... Ts>
class StopFlashAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_flash(); }
};

template<typename... Ts>
class SetStatusAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  TEMPLATABLE_VALUE(std::string, status)
  TEMPLATABLE_VALUE(uint32_t, duration)

  void play(const Ts &...x) override {
    const std::string status = this->status_.value(x...);
    this->parent_->set_status(status.c_str(), this->duration_.value(x...));
  }
};

template<typename... Ts>
class SetMaxBrightnessAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  TEMPLATABLE_VALUE(float, max_brightness)

  void play(const Ts &...x) override { this->parent_->set_max_brightness(this->max_brightness_.value(x...)); }
};

template<typename... Ts>
class LockAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  void play(const Ts &...x) override { this->parent_->set_locked(true); }
};

template<typename... Ts>
class UnlockAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  void play(const Ts &...x) override { this->parent_->set_locked(false); }
};

template<typename... Ts>
class ResetPressCountAction : public Action<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  void play(const Ts &...x) override { this->parent_->reset_press_count(); }
};

template<typename... Ts>
class IsPressedCondition : public Condition<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_pressed(); }
};

template<typename... Ts>
class IsLockedCondition : public Condition<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_locked(); }
};

template<typename... Ts>
class IsConnectedCondition : public Condition<Ts...>, public Parented<DFRobotRGBButton> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_connected(); }
};

}  // namespace esphome::dfrobot_rgb_button
