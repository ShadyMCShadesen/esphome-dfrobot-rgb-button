#pragma once

#include <cstdint>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_EVENT
#include "esphome/components/event/event.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif

namespace esphome::dfrobot_rgb_button {

// ---------------------------------------------------------------------------
// Register map. Source: DFRobot_RGBButton.h (github.com/DFRobot/DFRobot_RGBButton, V1.0).
// These are the only registers the vendor library defines; nothing else is used.
// ---------------------------------------------------------------------------
static const uint8_t REG_I2C_ADDR = 0x00;  // RGBBUTTON_I2C_ADDR_REG (documented, not used by this component)
static const uint8_t REG_RED = 0x01;       // RGBBUTTON_RED_REG, followed by GREEN (0x02) and BLUE (0x03)
static const uint8_t REG_BUTTON = 0x04;    // RGBBUTTON_BUTTON_SIGNAL_REG, non-zero while pressed
static const uint8_t REG_PID_MSB = 0x09;   // RGBBUTTON_PID_MSB_REG, followed by PID LSB (0x0A)
static const uint16_t PART_ID = 0x43DF;    // RGBBUTTON_PART_ID

// ---------------------------------------------------------------------------
// Tunables that are implementation decisions rather than vendor facts.
// ---------------------------------------------------------------------------
/// Consecutive failed I2C transactions before the module is considered disconnected.
static const uint8_t MAX_CONSECUTIVE_ERRORS = 3;
/// Interval between re-detection attempts while the module is disconnected.
static const uint32_t RECONNECT_INTERVAL_MS = 1000;
/// Interrupt mode only: slow safety poll that doubles as communication watchdog.
static const uint32_t INTERRUPT_KEEPALIVE_MS = 1000;
/// Interrupt mode only: after an edge the register is re-read every loop for this long.
static const uint32_t INTERRUPT_SETTLE_MS = 50;
/// Lower bound between two LED writes; caps bus load during transitions and effects.
static const uint32_t LED_MIN_WRITE_INTERVAL_MS = 15;
/// Length of the optional boot animation.
static const uint32_t BOOT_ANIMATION_MS = 1500;

struct RGBFloat {
  float r{0.0f};
  float g{0.0f};
  float b{0.0f};
};

enum StatusEffect : uint8_t {
  STATUS_EFFECT_SOLID = 0,
  STATUS_EFFECT_BLINK = 1,
  STATUS_EFFECT_BREATHE = 2,
};

struct StatusDefinition {
  const char *name;
  RGBFloat color;
  StatusEffect effect;
  uint32_t period_ms;
};

enum ClickType : uint8_t {
  CLICK_SINGLE = 0,
  CLICK_DOUBLE = 1,
  CLICK_LONG = 2,
};

enum ConnectionStatusMode : uint8_t {
  CONNECTION_STATUS_NONE = 0,
  CONNECTION_STATUS_NETWORK = 1,
  CONNECTION_STATUS_API = 2,
};

enum ToggleEvent : uint8_t {
  TOGGLE_EVENT_NONE = 0,
  TOGGLE_EVENT_PRESS = 1,
  TOGGLE_EVENT_RELEASE = 2,
  TOGGLE_EVENT_CLICK = 3,
  TOGGLE_EVENT_DOUBLE_CLICK = 4,
  TOGGLE_EVENT_LONG_PRESS = 5,
};

class DFRobotRGBButton : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ----- configuration (called from generated code) -----
  void set_update_interval(uint32_t update_interval) { this->update_interval_ms_ = update_interval; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_debounce(uint32_t debounce) { this->debounce_ms_ = debounce; }
  void set_long_press_time(uint32_t time) { this->long_press_ms_ = time; }
  void set_double_click_time(uint32_t time) { this->double_click_ms_ = time; }
  void set_press_feedback(float red, float green, float blue) {
    this->press_feedback_enabled_ = true;
    this->press_feedback_color_ = {red, green, blue};
  }
  void set_color_correct(float red, float green, float blue) { this->color_correct_ = {red, green, blue}; }
  void set_gamma_correct(float gamma) { this->gamma_ = gamma; }
  void add_status(const char *name, float red, float green, float blue, StatusEffect effect, uint32_t period_ms) {
    this->statuses_.push_back({name, {red, green, blue}, effect, period_ms});
  }
  void set_connection_status(ConnectionStatusMode mode, uint8_t status_index) {
    this->connection_status_mode_ = mode;
    this->connection_status_index_ = status_index;
  }
  void set_sync_source(DFRobotRGBButton *source) { this->sync_source_ = source; }
  void set_boot_animation(bool enabled) { this->boot_animation_enabled_ = enabled; }
  void set_led_refresh_interval(uint32_t interval) { this->led_refresh_ms_ = interval; }
  void set_press_count_restore_key(uint32_t key) {
    this->restore_press_count_ = true;
    this->press_count_key_ = key;
  }

  // ----- entities -----
#ifdef USE_BINARY_SENSOR
  void set_button_binary_sensor(binary_sensor::BinarySensor *sens) { this->button_binary_sensor_ = sens; }
  void set_connected_binary_sensor(binary_sensor::BinarySensor *sens) { this->connected_binary_sensor_ = sens; }
#endif
#ifdef USE_SENSOR
  void set_press_count_sensor(sensor::Sensor *sens) { this->press_count_sensor_ = sens; }
  void set_last_press_duration_sensor(sensor::Sensor *sens) { this->last_press_duration_sensor_ = sens; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_click_text_sensor(text_sensor::TextSensor *sens) { this->last_click_text_sensor_ = sens; }
#endif
#ifdef USE_EVENT
  void set_click_event(event::Event *event) { this->click_event_ = event; }
#endif
#ifdef USE_SELECT
  void set_status_select(select::Select *sel) { this->status_select_ = sel; }
#endif
#ifdef USE_SWITCH
  void set_lock_switch(switch_::Switch *sw) { this->lock_switch_ = sw; }
#endif
#ifdef USE_NUMBER
  void set_max_brightness_number(number::Number *num) { this->max_brightness_number_ = num; }
#endif
#ifdef USE_LIGHT
  void set_light_state(light::LightState *state) { this->light_state_ = state; }
  void set_toggle_event(ToggleEvent event) { this->toggle_event_ = event; }
  void set_auto_off(uint32_t auto_off) { this->auto_off_ms_ = auto_off; }
  bool has_light() const { return this->light_state_ != nullptr; }
#endif

  // ----- runtime API (actions, lambdas, sub-entities) -----
  /// Base LED color as produced by the light platform. Values are 0..1 with gamma already applied.
  void set_light_color(float red, float green, float blue) { this->light_color_ = {red, green, blue}; }
  /// Set the color of the light entity (if configured), turning it on. Used by color presets.
  void apply_light_color(float red, float green, float blue);
  /// Flash the LED `count` times, then return to whatever was shown before. Non-blocking.
  void flash(float red, float green, float blue, uint32_t count, uint32_t on_ms, uint32_t off_ms);
  /// Abort a running flash sequence.
  void stop_flash() { this->flash_remaining_ = 0; }
  /// Select a status by index (0 = idle). A non-zero duration reverts to idle afterwards.
  void set_status(uint8_t index, uint32_t duration_ms = 0);
  /// Select a status by name. Returns false (and logs a warning) if the name is unknown.
  bool set_status(const char *name, uint32_t duration_ms = 0);
  uint8_t get_status_index() const { return this->status_index_; }
  const char *get_status_name() const { return this->statuses_[this->status_index_].name; }
  /// Night mode: scale the final LED output, 0..1.
  void set_max_brightness(float max_brightness);
  float get_max_brightness() const { return this->max_brightness_; }
  void set_locked(bool locked);
  bool is_locked() const { return this->locked_; }
  bool is_pressed() const { return this->pressed_; }
  bool is_connected() const { return this->connected_; }
  uint32_t get_press_count() const { return this->press_count_; }
  void reset_press_count();

  // ----- triggers -----
  Trigger<> *get_press_trigger() { return &this->press_trigger_; }
  Trigger<uint32_t> *get_release_trigger() { return &this->release_trigger_; }
  Trigger<> *get_click_trigger() { return &this->click_trigger_; }
  Trigger<> *get_double_click_trigger() { return &this->double_click_trigger_; }
  Trigger<> *get_long_press_trigger() { return &this->long_press_trigger_; }
  Trigger<> *get_locked_press_trigger() { return &this->locked_press_trigger_; }
  Trigger<> *get_connect_trigger() { return &this->connect_trigger_; }
  Trigger<> *get_disconnect_trigger() { return &this->disconnect_trigger_; }

 protected:
  enum ProbeResult : uint8_t {
    PROBE_OK = 0,
    PROBE_NO_RESPONSE = 1,
    PROBE_WRONG_ID = 2,
    PROBE_NOT_RUN = 0xFF,
  };

  static void gpio_intr(DFRobotRGBButton *arg);

  bool read_register_(uint8_t reg, uint8_t *data, size_t len);
  bool probe_();
  void set_connected_(bool connected);
  void handle_comm_error_();

  void poll_button_(uint32_t now);
  void handle_button_change_(bool pressed, uint32_t now);
  void finish_press_(uint32_t now, bool cancelled);
  void process_clicks_(uint32_t now);
  void emit_click_(ClickType type);
  void toggle_light_(ToggleEvent event);
  void process_light_(uint32_t now);
  void publish_press_count_();

  uint8_t effective_status_index_(uint32_t now);
  RGBFloat render_status_(const StatusDefinition &status, uint32_t now) const;
  RGBFloat gamma_correct_(const RGBFloat &color, float level = 1.0f) const;
  RGBFloat compose_(uint32_t now);
  void update_led_(uint32_t now);

  // configuration
  InternalGPIOPin *interrupt_pin_{nullptr};
  DFRobotRGBButton *sync_source_{nullptr};
  uint32_t update_interval_ms_{20};
  uint32_t debounce_ms_{0};
  uint32_t long_press_ms_{800};
  uint32_t double_click_ms_{300};
  uint32_t led_refresh_ms_{0};
  uint32_t press_count_key_{0};
  RGBFloat press_feedback_color_{1.0f, 1.0f, 1.0f};
  RGBFloat color_correct_{1.0f, 1.0f, 1.0f};
  float gamma_{2.8f};
  float max_brightness_{1.0f};
  std::vector<StatusDefinition> statuses_;
  ConnectionStatusMode connection_status_mode_{CONNECTION_STATUS_NONE};
  uint8_t connection_status_index_{0};
  bool press_feedback_enabled_{false};
  bool boot_animation_enabled_{false};
  bool restore_press_count_{false};

  // communication state
  ProbeResult last_probe_result_{PROBE_NOT_RUN};
  uint32_t last_probe_{0};
  uint32_t last_poll_{0};
  uint8_t consecutive_errors_{0};
  bool connected_{false};

  // button state
  volatile bool isr_flag_{false};
  uint32_t settle_until_{0};
  uint32_t raw_changed_at_{0};
  uint32_t press_start_{0};
  uint32_t last_release_{0};
  uint32_t press_count_{0};
  uint8_t pending_clicks_{0};
  bool settling_{false};
  bool raw_state_{false};
  bool debounced_state_{false};
  bool pressed_{false};
  bool long_fired_{false};
  bool locked_{false};
  ESPPreferenceObject press_count_pref_;

  // LED state
  RGBFloat light_color_{};
  RGBFloat composed_{};
  RGBFloat flash_color_{};
  uint32_t flash_remaining_{0};
  uint32_t flash_on_ms_{0};
  uint32_t flash_off_ms_{0};
  uint32_t flash_phase_start_{0};
  uint32_t status_until_{0};
  uint32_t boot_animation_start_{0};
  uint32_t last_led_write_{0};
  uint8_t last_written_[3]{0, 0, 0};
  uint8_t status_index_{0};
  bool flash_on_phase_{false};
  bool status_timed_{false};
  bool boot_animation_active_{false};
  bool boot_animation_done_{false};
  bool force_led_write_{true};

  // entities
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *button_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *press_count_sensor_{nullptr};
  sensor::Sensor *last_press_duration_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_click_text_sensor_{nullptr};
#endif
#ifdef USE_EVENT
  event::Event *click_event_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *status_select_{nullptr};
#endif
#ifdef USE_SWITCH
  switch_::Switch *lock_switch_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *max_brightness_number_{nullptr};
#endif
#ifdef USE_LIGHT
  light::LightState *light_state_{nullptr};
  uint32_t auto_off_ms_{0};
  uint32_t light_on_since_{0};
  ToggleEvent toggle_event_{TOGGLE_EVENT_NONE};
  bool light_was_on_{false};
#endif

  // triggers
  Trigger<> press_trigger_;
  Trigger<uint32_t> release_trigger_;
  Trigger<> click_trigger_;
  Trigger<> double_click_trigger_;
  Trigger<> long_press_trigger_;
  Trigger<> locked_press_trigger_;
  Trigger<> connect_trigger_;
  Trigger<> disconnect_trigger_;
};

}  // namespace esphome::dfrobot_rgb_button
