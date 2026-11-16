#include "dfrobot_rgb_button.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::dfrobot_rgb_button {

static const char *const TAG = "dfrobot_rgb_button";

static const float TAU_F = 6.28318530718f;

static const char *click_type_to_string(ClickType type) {
  switch (type) {
    case CLICK_SINGLE:
      return "single";
    case CLICK_DOUBLE:
      return "double";
    case CLICK_LONG:
      return "long";
    default:
      return "unknown";
  }
}

static const char *status_effect_to_string(StatusEffect effect) {
  switch (effect) {
    case STATUS_EFFECT_BLINK:
      return "blink";
    case STATUS_EFFECT_BREATHE:
      return "breathe";
    case STATUS_EFFECT_SOLID:
    default:
      return "solid";
  }
}

static uint8_t to_byte(float value) {
  if (!(value > 0.0f))
    return 0;
  if (value >= 1.0f)
    return 255;
  return static_cast<uint8_t>(lroundf(value * 255.0f));
}

// The ISR only sets a flag; all I2C traffic happens in loop().
void IRAM_ATTR DFRobotRGBButton::gpio_intr(DFRobotRGBButton *arg) { arg->isr_flag_ = true; }

// ---------------------------------------------------------------------------
// Setup / config dump
// ---------------------------------------------------------------------------

void DFRobotRGBButton::setup() {
  if (this->statuses_.empty()) {
    // Generated code always registers "idle" first; keep index 0 valid if someone builds the object by hand.
    this->add_status("idle", 0.0f, 0.0f, 0.0f, STATUS_EFFECT_SOLID, 0);
  }

  if (this->restore_press_count_) {
    this->press_count_pref_ = global_preferences->make_preference<uint32_t>(this->press_count_key_);
    uint32_t stored;
    if (this->press_count_pref_.load(&stored))
      this->press_count_ = stored;
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(DFRobotRGBButton::gpio_intr, this, gpio::INTERRUPT_ANY_EDGE);
  }

  const uint32_t now = millis();
  this->last_probe_ = now;
  if (!this->probe_()) {
    // Not fatal: the module may be hot-plugged later, loop() keeps probing.
    this->status_set_warning();
  }

#ifdef USE_BINARY_SENSOR
  if (this->button_binary_sensor_ != nullptr)
    this->button_binary_sensor_->publish_state(false);
  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_state(this->connected_);
#endif
#ifdef USE_SELECT
  if (this->status_select_ != nullptr)
    this->status_select_->publish_state(static_cast<size_t>(this->status_index_));
#endif
#ifdef USE_NUMBER
  if (this->max_brightness_number_ != nullptr && !this->max_brightness_number_->has_state())
    this->max_brightness_number_->publish_state(this->max_brightness_ * 100.0f);
#endif
  this->publish_press_count_();
}

void DFRobotRGBButton::dump_config() {
  ESP_LOGCONFIG(TAG, "DFRobot RGB LED Button (DFR0991):");
  LOG_I2C_DEVICE(this);
  if (this->connected_) {
    ESP_LOGCONFIG(TAG, "  Product ID: 0x%04X (OK)", PART_ID);
  } else if (this->last_probe_result_ == PROBE_WRONG_ID) {
    ESP_LOGE(TAG, "  A device answered, but its product ID is not 0x%04X", PART_ID);
  } else {
    ESP_LOGE(TAG, "  Module not responding; check wiring, DIP switch address and power");
  }
  if (this->interrupt_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Button detection: interrupt (keep-alive poll every %" PRIu32 " ms)", INTERRUPT_KEEPALIVE_MS);
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Button detection: polling every %" PRIu32 " ms", this->update_interval_ms_);
  }
  ESP_LOGCONFIG(TAG,
                "  Debounce: %" PRIu32
                " ms\n"
                "  Long press time: %" PRIu32
                " ms\n"
                "  Double click time: %" PRIu32
                " ms%s\n"
                "  Locked: %s\n"
                "  Max brightness: %.0f%%\n"
                "  Gamma correct (status/flash/feedback colors): %.2f\n"
                "  Color correct: R %.0f%%, G %.0f%%, B %.0f%%",
                this->debounce_ms_, this->long_press_ms_, this->double_click_ms_,
                this->double_click_ms_ == 0 ? " (double click detection disabled)" : "", YESNO(this->locked_),
                this->max_brightness_ * 100.0f, this->gamma_, this->color_correct_.r * 100.0f,
                this->color_correct_.g * 100.0f, this->color_correct_.b * 100.0f);
  if (this->press_feedback_enabled_) {
    ESP_LOGCONFIG(TAG, "  Press feedback: R %.0f%%, G %.0f%%, B %.0f%%", this->press_feedback_color_.r * 100.0f,
                  this->press_feedback_color_.g * 100.0f, this->press_feedback_color_.b * 100.0f);
  }
  if (this->led_refresh_ms_ != 0) {
    ESP_LOGCONFIG(TAG, "  LED refresh interval: %" PRIu32 " ms", this->led_refresh_ms_);
  }
  if (this->sync_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  LED mirrors the module at address 0x%02X", this->sync_source_->get_i2c_address());
  }
  if (this->connection_status_mode_ != CONNECTION_STATUS_NONE) {
    ESP_LOGCONFIG(TAG, "  Connection status: shows '%s' while %s is disconnected",
                  this->statuses_[this->connection_status_index_].name,
                  this->connection_status_mode_ == CONNECTION_STATUS_API ? "the API" : "the network");
  }
  ESP_LOGCONFIG(TAG,
                "  Boot animation: %s\n"
                "  Press counter: %" PRIu32
                " (restore: %s)\n"
                "  Statuses:",
                YESNO(this->boot_animation_enabled_), this->press_count_, YESNO(this->restore_press_count_));
  for (const auto &status : this->statuses_) {
    if (&status == &this->statuses_[0]) {
      ESP_LOGCONFIG(TAG, "    - %s (shows the light)", status.name);
    } else {
      ESP_LOGCONFIG(TAG, "    - %s: R %.0f%%, G %.0f%%, B %.0f%%, %s, period %" PRIu32 " ms", status.name,
                    status.color.r * 100.0f, status.color.g * 100.0f, status.color.b * 100.0f,
                    status_effect_to_string(status.effect), status.period_ms);
    }
  }
#ifdef USE_LIGHT
  if (this->light_state_ != nullptr) {
    ESP_LOGCONFIG(TAG,
                  "  Light: '%s'\n"
                  "    Auto-off: %" PRIu32
                  " ms%s\n"
                  "    Toggle mode: %s",
                  this->light_state_->get_name().c_str(), this->auto_off_ms_,
                  this->auto_off_ms_ == 0 ? " (disabled)" : "", YESNO(this->toggle_event_ != TOGGLE_EVENT_NONE));
  }
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Button", this->button_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Press Count", this->press_count_sensor_);
  LOG_SENSOR("  ", "Last Press Duration", this->last_press_duration_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Last Click", this->last_click_text_sensor_);
#endif
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void DFRobotRGBButton::loop() {
  const uint32_t now = millis();

  if (!this->connected_) {
    if (now - this->last_probe_ >= RECONNECT_INTERVAL_MS) {
      this->last_probe_ = now;
      this->probe_();
    }
  } else {
    this->poll_button_(now);
  }

  this->process_clicks_(now);
  this->process_light_(now);
  this->update_led_(now);
}

// ---------------------------------------------------------------------------
// I2C communication, detection and watchdog
// ---------------------------------------------------------------------------

bool DFRobotRGBButton::read_register_(uint8_t reg, uint8_t *data, size_t len) {
  // DFRobot's readReg() finishes the register-pointer write with a STOP condition and reads in a
  // separate transaction. Whether the module's firmware accepts a repeated START is not documented,
  // so the verified sequence is reproduced here instead of using I2CDevice::read_register().
  if (this->write(&reg, 1) != i2c::ERROR_OK)
    return false;
  return this->read(data, len) == i2c::ERROR_OK;
}

bool DFRobotRGBButton::probe_() {
  uint8_t id[2];
  ProbeResult result = PROBE_OK;
  if (!this->read_register_(REG_PID_MSB, id, sizeof(id))) {
    result = PROBE_NO_RESPONSE;
  } else if (encode_uint16(id[0], id[1]) != PART_ID) {
    result = PROBE_WRONG_ID;
  }

  // Only log when the outcome changes, the probe repeats every second while disconnected.
  if (result != this->last_probe_result_) {
    this->last_probe_result_ = result;
    if (result == PROBE_NO_RESPONSE) {
      ESP_LOGW(TAG, "No response from module at 0x%02X; check wiring and DIP switch. Retrying every %" PRIu32 " ms",
               this->get_i2c_address(), RECONNECT_INTERVAL_MS);
    } else if (result == PROBE_WRONG_ID) {
      ESP_LOGE(TAG, "Device at 0x%02X reports product ID 0x%04X, expected 0x%04X; not a DFR0991?",
               this->get_i2c_address(), encode_uint16(id[0], id[1]), PART_ID);
    }
  }

  if (result != PROBE_OK)
    return false;

  this->set_connected_(true);
  return true;
}

void DFRobotRGBButton::set_connected_(bool connected) {
  if (this->connected_ == connected)
    return;
  this->connected_ = connected;
  this->consecutive_errors_ = 0;
  const uint32_t now = millis();

  if (connected) {
    ESP_LOGI(TAG, "Module at 0x%02X connected", this->get_i2c_address());
    this->status_clear_warning();
    // The module powers up dark, so whatever should be shown has to be sent again.
    this->force_led_write_ = true;
    this->raw_state_ = false;
    this->debounced_state_ = false;
    this->last_poll_ = now - this->update_interval_ms_;
    if (this->boot_animation_enabled_ && !this->boot_animation_done_) {
      this->boot_animation_active_ = true;
      this->boot_animation_start_ = now;
    }
    this->boot_animation_done_ = true;
    this->connect_trigger_.trigger();
  } else {
    ESP_LOGW(TAG, "Module at 0x%02X stopped responding", this->get_i2c_address());
    this->last_probe_result_ = PROBE_NO_RESPONSE;
    this->last_probe_ = now;
    this->status_set_warning();
    this->settling_ = false;
    this->raw_state_ = false;
    this->debounced_state_ = false;
    if (this->pressed_)
      this->finish_press_(now, true);
    this->disconnect_trigger_.trigger();
  }
#ifdef USE_BINARY_SENSOR
  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_state(connected);
#endif
}

void DFRobotRGBButton::handle_comm_error_() {
  if (this->consecutive_errors_ < 0xFF)
    this->consecutive_errors_++;
  ESP_LOGV(TAG, "I2C error %u/%u at 0x%02X", this->consecutive_errors_, MAX_CONSECUTIVE_ERRORS,
           this->get_i2c_address());
  if (this->consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS)
    this->set_connected_(false);
}

// ---------------------------------------------------------------------------
// Button handling
// ---------------------------------------------------------------------------

void DFRobotRGBButton::poll_button_(uint32_t now) {
  bool due;
  if (this->interrupt_pin_ != nullptr) {
    if (this->isr_flag_) {
      this->isr_flag_ = false;
      this->settling_ = true;
      this->settle_until_ = now + INTERRUPT_SETTLE_MS;
    }
    const bool debounce_pending = this->raw_state_ != this->debounced_state_;
    if (this->settling_ && static_cast<int32_t>(now - this->settle_until_) >= 0)
      this->settling_ = false;
    due = this->settling_ || debounce_pending || (now - this->last_poll_ >= INTERRUPT_KEEPALIVE_MS);
  } else {
    due = now - this->last_poll_ >= this->update_interval_ms_;
  }
  if (!due)
    return;
  this->last_poll_ = now;

  uint8_t value;
  if (!this->read_register_(REG_BUTTON, &value, 1)) {
    this->handle_comm_error_();
    return;
  }
  this->consecutive_errors_ = 0;

  // The vendor library treats any non-zero value as "pressed".
  const bool raw = value != 0;
  if (raw != this->raw_state_) {
    this->raw_state_ = raw;
    this->raw_changed_at_ = now;
  }
  if (raw != this->debounced_state_ && now - this->raw_changed_at_ >= this->debounce_ms_) {
    this->debounced_state_ = raw;
    this->handle_button_change_(raw, now);
  }
}

void DFRobotRGBButton::handle_button_change_(bool pressed, uint32_t now) {
  if (!pressed) {
    // A release without a registered press belongs to a press swallowed by the lock.
    if (this->pressed_)
      this->finish_press_(now, false);
    return;
  }

  if (this->locked_) {
    ESP_LOGD(TAG, "Press ignored, button is locked");
    this->locked_press_trigger_.trigger();
    return;
  }

  this->pressed_ = true;
  this->press_start_ = now;
  this->long_fired_ = false;
  this->press_count_++;
  if (this->restore_press_count_)
    this->press_count_pref_.save(&this->press_count_);
  ESP_LOGD(TAG, "Button pressed (count: %" PRIu32 ")", this->press_count_);

#ifdef USE_BINARY_SENSOR
  if (this->button_binary_sensor_ != nullptr)
    this->button_binary_sensor_->publish_state(true);
#endif
  this->publish_press_count_();
  this->press_trigger_.trigger();
  this->toggle_light_(TOGGLE_EVENT_PRESS);
}

void DFRobotRGBButton::finish_press_(uint32_t now, bool cancelled) {
  const uint32_t duration = now - this->press_start_;
  this->pressed_ = false;
  ESP_LOGD(TAG, "Button released after %" PRIu32 " ms%s", duration, cancelled ? " (cancelled)" : "");

#ifdef USE_BINARY_SENSOR
  if (this->button_binary_sensor_ != nullptr)
    this->button_binary_sensor_->publish_state(false);
#endif
#ifdef USE_SENSOR
  if (this->last_press_duration_sensor_ != nullptr)
    this->last_press_duration_sensor_->publish_state(duration);
#endif
  this->release_trigger_.trigger(duration);

  if (cancelled) {
    // Lock engaged or module lost mid-press: no click is derived from this press.
    this->pending_clicks_ = 0;
    return;
  }
  this->toggle_light_(TOGGLE_EVENT_RELEASE);

  if (this->long_fired_)
    return;

  this->pending_clicks_++;
  this->last_release_ = now;
  if (this->double_click_ms_ == 0) {
    this->pending_clicks_ = 0;
    this->emit_click_(CLICK_SINGLE);
  } else if (this->pending_clicks_ >= 2) {
    this->pending_clicks_ = 0;
    this->emit_click_(CLICK_DOUBLE);
  }
}

void DFRobotRGBButton::process_clicks_(uint32_t now) {
  if (this->pressed_) {
    if (!this->long_fired_ && now - this->press_start_ >= this->long_press_ms_) {
      this->long_fired_ = true;
      if (this->pending_clicks_ > 0) {
        // A click followed by a long press: report the click first, it is not half of a double click.
        this->pending_clicks_ = 0;
        this->emit_click_(CLICK_SINGLE);
      }
      this->emit_click_(CLICK_LONG);
    }
    return;
  }
  if (this->pending_clicks_ == 1 && now - this->last_release_ >= this->double_click_ms_) {
    this->pending_clicks_ = 0;
    this->emit_click_(CLICK_SINGLE);
  }
}

void DFRobotRGBButton::emit_click_(ClickType type) {
  const char *name = click_type_to_string(type);
  ESP_LOGD(TAG, "Click detected: %s", name);
#ifdef USE_TEXT_SENSOR
  if (this->last_click_text_sensor_ != nullptr)
    this->last_click_text_sensor_->publish_state(name);
#endif
#ifdef USE_EVENT
  if (this->click_event_ != nullptr)
    this->click_event_->trigger(name);
#endif
  switch (type) {
    case CLICK_SINGLE:
      this->click_trigger_.trigger();
      this->toggle_light_(TOGGLE_EVENT_CLICK);
      break;
    case CLICK_DOUBLE:
      this->double_click_trigger_.trigger();
      this->toggle_light_(TOGGLE_EVENT_DOUBLE_CLICK);
      break;
    case CLICK_LONG:
      this->long_press_trigger_.trigger();
      this->toggle_light_(TOGGLE_EVENT_LONG_PRESS);
      break;
  }
}

void DFRobotRGBButton::toggle_light_(ToggleEvent event) {
#ifdef USE_LIGHT
  if (this->light_state_ == nullptr || this->toggle_event_ == TOGGLE_EVENT_NONE || this->toggle_event_ != event)
    return;
  auto call = this->light_state_->toggle();
  call.perform();
#endif
}

void DFRobotRGBButton::process_light_(uint32_t now) {
#ifdef USE_LIGHT
  if (this->light_state_ == nullptr || this->auto_off_ms_ == 0)
    return;
  const bool on = this->light_state_->remote_values.is_on();
  if (on && !this->light_was_on_)
    this->light_on_since_ = now;
  this->light_was_on_ = on;
  if (on && now - this->light_on_since_ >= this->auto_off_ms_) {
    ESP_LOGD(TAG, "Auto-off timer expired, turning light off");
    this->light_was_on_ = false;
    auto call = this->light_state_->turn_off();
    call.perform();
  }
#endif
}

void DFRobotRGBButton::publish_press_count_() {
#ifdef USE_SENSOR
  if (this->press_count_sensor_ != nullptr)
    this->press_count_sensor_->publish_state(this->press_count_);
#endif
}

void DFRobotRGBButton::reset_press_count() {
  this->press_count_ = 0;
  if (this->restore_press_count_)
    this->press_count_pref_.save(&this->press_count_);
  this->publish_press_count_();
}

void DFRobotRGBButton::set_locked(bool locked) {
  if (this->locked_ != locked) {
    this->locked_ = locked;
    ESP_LOGD(TAG, "Button %s", locked ? "locked" : "unlocked");
    if (locked && this->pressed_)
      this->finish_press_(millis(), true);
    if (locked)
      this->pending_clicks_ = 0;
  }
#ifdef USE_SWITCH
  if (this->lock_switch_ != nullptr)
    this->lock_switch_->publish_state(locked);
#endif
}

// ---------------------------------------------------------------------------
// LED features
// ---------------------------------------------------------------------------

void DFRobotRGBButton::apply_light_color(float red, float green, float blue) {
#ifdef USE_LIGHT
  if (this->light_state_ != nullptr) {
    auto call = this->light_state_->turn_on();
    call.set_rgb(red, green, blue);
    call.perform();
    return;
  }
#endif
  ESP_LOGW(TAG, "Cannot apply color: no 'dfrobot_rgb_button' light is configured for this module");
}

void DFRobotRGBButton::flash(float red, float green, float blue, uint32_t count, uint32_t on_ms, uint32_t off_ms) {
  if (count == 0 || on_ms == 0) {
    this->flash_remaining_ = 0;
    return;
  }
  this->flash_color_ = {red, green, blue};
  this->flash_remaining_ = count;
  this->flash_on_ms_ = on_ms;
  this->flash_off_ms_ = off_ms;
  this->flash_on_phase_ = true;
  this->flash_phase_start_ = millis();
}

void DFRobotRGBButton::set_status(uint8_t index, uint32_t duration_ms) {
  if (index >= this->statuses_.size()) {
    ESP_LOGW(TAG, "Invalid status index %u", index);
    return;
  }
  this->status_index_ = index;
  this->status_timed_ = duration_ms != 0 && index != 0;
  this->status_until_ = millis() + duration_ms;
  ESP_LOGD(TAG, "Status set to '%s'", this->statuses_[index].name);
#ifdef USE_SELECT
  if (this->status_select_ != nullptr)
    this->status_select_->publish_state(static_cast<size_t>(index));
#endif
}

bool DFRobotRGBButton::set_status(const char *name, uint32_t duration_ms) {
  for (size_t i = 0; i < this->statuses_.size(); i++) {
    if (strcmp(this->statuses_[i].name, name) == 0) {
      this->set_status(static_cast<uint8_t>(i), duration_ms);
      return true;
    }
  }
  ESP_LOGW(TAG, "Unknown status '%s'", name);
  return false;
}

void DFRobotRGBButton::set_max_brightness(float max_brightness) {
  this->max_brightness_ = std::clamp(max_brightness, 0.0f, 1.0f);
#ifdef USE_NUMBER
  if (this->max_brightness_number_ != nullptr)
    this->max_brightness_number_->publish_state(roundf(this->max_brightness_ * 100.0f));
#endif
}

uint8_t DFRobotRGBButton::effective_status_index_(uint32_t now) {
  if (this->status_timed_ && static_cast<int32_t>(now - this->status_until_) >= 0)
    this->set_status(static_cast<uint8_t>(0));

  bool link_down = false;
  switch (this->connection_status_mode_) {
    case CONNECTION_STATUS_NETWORK:
#ifdef USE_NETWORK
      link_down = !network::is_connected();
#endif
      break;
    case CONNECTION_STATUS_API:
#ifdef USE_API
      link_down = api::global_api_server == nullptr || !api::global_api_server->is_connected();
#endif
      break;
    case CONNECTION_STATUS_NONE:
    default:
      break;
  }
  return link_down ? this->connection_status_index_ : this->status_index_;
}

RGBFloat DFRobotRGBButton::gamma_correct_(const RGBFloat &color, float level) const {
  auto channel = [this, level](float value) -> float {
    value *= level;
    if (value <= 0.0f)
      return 0.0f;
    if (value >= 1.0f)
      return 1.0f;
    return this->gamma_ > 0.0f ? powf(value, this->gamma_) : value;
  };
  return {channel(color.r), channel(color.g), channel(color.b)};
}

RGBFloat DFRobotRGBButton::render_status_(const StatusDefinition &status, uint32_t now) const {
  // Phases are derived from the absolute uptime, so modules showing the same status stay in step.
  switch (status.effect) {
    case STATUS_EFFECT_BLINK: {
      if (status.period_ms == 0 || (now % status.period_ms) < status.period_ms / 2)
        return this->gamma_correct_(status.color);
      return {};
    }
    case STATUS_EFFECT_BREATHE: {
      if (status.period_ms == 0)
        return this->gamma_correct_(status.color);
      const float phase = static_cast<float>(now % status.period_ms) / static_cast<float>(status.period_ms);
      const float level = 0.08f + 0.92f * (0.5f - 0.5f * cosf(TAU_F * phase));
      return this->gamma_correct_(status.color, level);
    }
    case STATUS_EFFECT_SOLID:
    default:
      return this->gamma_correct_(status.color);
  }
}

RGBFloat DFRobotRGBButton::compose_(uint32_t now) {
  RGBFloat color;

  // Layer 1: base. Either a mirror of another module or this module's light / status.
  const uint8_t status_index = this->effective_status_index_(now);
  if (this->sync_source_ != nullptr) {
    color = this->sync_source_->composed_;
  } else if (status_index != 0) {
    color = this->render_status_(this->statuses_[status_index], now);
  } else {
    color = this->light_color_;
  }

  // Layer 2: press feedback.
  if (this->press_feedback_enabled_ && this->pressed_)
    color = this->gamma_correct_(this->press_feedback_color_);

  // Layer 3: flash notification.
  if (this->flash_remaining_ > 0) {
    const uint32_t elapsed = now - this->flash_phase_start_;
    if (this->flash_on_phase_) {
      if (elapsed >= this->flash_on_ms_) {
        this->flash_on_phase_ = false;
        this->flash_phase_start_ = now;
        this->flash_remaining_--;
      }
    } else if (elapsed >= this->flash_off_ms_) {
      this->flash_on_phase_ = true;
      this->flash_phase_start_ = now;
    }
    if (this->flash_remaining_ > 0)
      color = this->flash_on_phase_ ? this->gamma_correct_(this->flash_color_) : RGBFloat{};
  }

  // Layer 4: boot animation (one hue sweep with a fade-in/fade-out envelope).
  if (this->boot_animation_active_) {
    const uint32_t elapsed = now - this->boot_animation_start_;
    if (elapsed >= BOOT_ANIMATION_MS) {
      this->boot_animation_active_ = false;
    } else {
      const float progress = static_cast<float>(elapsed) / static_cast<float>(BOOT_ANIMATION_MS);
      RGBFloat rainbow;
      hsv_to_rgb(static_cast<int>(progress * 359.0f), 1.0f, 1.0f, rainbow.r, rainbow.g, rainbow.b);
      color = this->gamma_correct_(rainbow, sinf(progress * TAU_F * 0.5f));
    }
  }

  return color;
}

void DFRobotRGBButton::update_led_(uint32_t now) {
  // compose_() also advances the flash / boot state machines, so it runs even while disconnected.
  this->composed_ = this->compose_(now);
  if (!this->connected_)
    return;

  // Night mode is specified perceptually, so it goes through the same gamma curve as the colors.
  float scale = this->max_brightness_;
  if (this->gamma_ > 0.0f && scale > 0.0f && scale < 1.0f)
    scale = powf(scale, this->gamma_);

  const uint8_t out[3] = {
      to_byte(this->composed_.r * scale * this->color_correct_.r),
      to_byte(this->composed_.g * scale * this->color_correct_.g),
      to_byte(this->composed_.b * scale * this->color_correct_.b),
  };

  const bool changed = memcmp(out, this->last_written_, sizeof(out)) != 0;
  const bool refresh = this->led_refresh_ms_ != 0 && now - this->last_led_write_ >= this->led_refresh_ms_;
  if (!this->force_led_write_ && !refresh) {
    if (!changed)
      return;
    if (now - this->last_led_write_ < LED_MIN_WRITE_INTERVAL_MS)
      return;  // rate limit; the pending change is picked up on a later loop
  }

  // Same sequence as DFRobot's setRGBColor(): register pointer 0x01 followed by R, G, B.
  if (this->write_register(REG_RED, out, sizeof(out)) != i2c::ERROR_OK) {
    this->handle_comm_error_();
    return;
  }
  this->consecutive_errors_ = 0;
  memcpy(this->last_written_, out, sizeof(out));
  this->last_led_write_ = now;
  this->force_led_write_ = false;
  ESP_LOGV(TAG, "LED 0x%02X <- R %u, G %u, B %u", this->get_i2c_address(), out[0], out[1], out[2]);
}

}  // namespace esphome::dfrobot_rgb_button
