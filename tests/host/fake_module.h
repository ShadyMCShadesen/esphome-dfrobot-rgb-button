#pragma once

// Simulated DFR0991 module for the host-platform test (tests/host/simulation.yaml).
//
// It models exactly what the vendor library relies on: a register pointer set by the first
// written byte, RGB registers 0x01-0x03, the button register 0x04 and the product ID in
// 0x09/0x0A. Nothing here is taken from real hardware measurements.

#include <cstdint>
#include <cstring>

#include "esphome/components/i2c/i2c_bus.h"

namespace dfr0991_sim {

class FakeModule : public esphome::i2c::I2CBus {
 public:
  FakeModule() { this->power_cycle(); }

  /// Reset the registers like a module that just powered up (LED dark).
  void power_cycle() {
    memset(this->regs, 0, sizeof(this->regs));
    this->regs[0x00] = this->address;
    this->regs[0x09] = 0x43;
    this->regs[0x0A] = 0xDF;
    this->pointer = 0;
  }

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                      uint8_t *read_buffer, size_t read_count) override {
    if (!this->present || address != this->address)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    this->transactions++;
    if (write_count > 0) {
      this->pointer = write_buffer[0];
      if (write_count > 1)
        this->led_writes++;
      for (size_t i = 1; i < write_count; i++)
        this->regs[this->pointer++ & 0x0F] = write_buffer[i];
    }
    this->regs[0x04] = this->pressed ? 1 : 0;
    for (size_t i = 0; i < read_count; i++)
      read_buffer[i] = this->regs[this->pointer++ & 0x0F];
    return esphome::i2c::ERROR_OK;
  }

  bool led_is(uint8_t red, uint8_t green, uint8_t blue) const {
    return this->regs[1] == red && this->regs[2] == green && this->regs[3] == blue;
  }

  uint8_t address{0x2A};
  uint8_t regs[16];
  uint8_t pointer{0};
  bool present{true};
  bool pressed{false};
  uint32_t led_writes{0};
  uint32_t transactions{0};
};

struct Counters {
  int press{0};
  int release{0};
  int click{0};
  int double_click{0};
  int long_press{0};
  int locked_press{0};
  int connect{0};
  int disconnect{0};
  int failures{0};
  int checks{0};
};

inline FakeModule &module() {
  static FakeModule instance;
  return instance;
}

inline Counters &counters() {
  static Counters instance;
  return instance;
}

}  // namespace dfr0991_sim

#define SIM_CHECK(condition)                                                                \
  do {                                                                                      \
    dfr0991_sim::counters().checks++;                                                       \
    if (condition) {                                                                        \
      ESP_LOGI("sim", "PASS: %s", #condition);                                              \
    } else {                                                                                \
      dfr0991_sim::counters().failures++;                                                   \
      ESP_LOGE("sim", "FAIL: %s (LED %u/%u/%u)", #condition, dfr0991_sim::module().regs[1], \
               dfr0991_sim::module().regs[2], dfr0991_sim::module().regs[3]);               \
    }                                                                                       \
  } while (false)
