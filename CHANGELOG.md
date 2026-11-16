# Changelog

All notable changes to this project are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-11-16

### Added

- Hub component `dfrobot_rgb_button` with native ESPHome I2C communication (no Arduino
  library), product ID check, address validation for the DIP switch range `0x23`–`0x2A`
  and support for multiple modules.
- Button detection by polling (default, `update_interval: 20ms`) or via the optional INT pin
  (`interrupt_pin`), with configurable `debounce`.
- Software click detection: `on_press`, `on_release`, `on_click`, `on_double_click`,
  `on_long_press`.
- `light` platform (RGB, transitions, `restore_mode`, all standard effects) with the software
  effects `dfrobot_breathe` and `dfrobot_color_cycle`, `toggle_mode` and `auto_off`.
- `binary_sensor` platform: `button`, `connected`.
- `sensor` platform: `press_count` (optional `restore`), `last_press_duration`.
- `text_sensor` platform: `last_click`.
- `event` platform: `click` with the event types `single`, `double`, `long`.
- `select` platform: `status`, `preset`.
- `switch` platform: `lock` (child lock) with `on_locked_press`.
- `number` platform: `max_brightness` (night mode).
- LED features: `press_feedback`, statuses with `solid` / `blink` / `breathe`, automatic
  `connection_status`, `gamma_correct`, `color_correct`, `sync_with`, `boot_animation`,
  `led_refresh_interval`.
- Actions `dfrobot_rgb_button.flash`, `.stop_flash`, `.set_status`, `.set_max_brightness`,
  `.lock`, `.unlock`, `.reset_press_count`; conditions `.is_pressed`, `.is_locked`,
  `.is_connected`.
- Communication watchdog with automatic re-detection (hot-plug), `on_connect` /
  `on_disconnect`.
- Examples, wiring and research documentation, CI (lint, config validation, compile matrix,
  simulated-module behaviour test).

[1.0.0]: https://github.com/ShadyMCShadesen/esphome-dfrobot-rgb-button/releases/tag/v1.0.0
