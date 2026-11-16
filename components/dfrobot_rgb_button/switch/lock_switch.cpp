#include "lock_switch.h"

#include "esphome/core/log.h"

namespace esphome::dfrobot_rgb_button {

static const char *const TAG = "dfrobot_rgb_button.switch";

void LockSwitch::setup() {
  // Fall back to the hub's `locked` option if the restore mode yields nothing.
  const bool initial = this->get_initial_state_with_restore_mode().value_or(this->parent_->is_locked());
  this->parent_->set_locked(initial);
}

void LockSwitch::dump_config() { LOG_SWITCH("", "DFRobot RGB Button Lock", this); }

void LockSwitch::write_state(bool state) {
  // The hub publishes the new state back to this entity.
  this->parent_->set_locked(state);
}

}  // namespace esphome::dfrobot_rgb_button
