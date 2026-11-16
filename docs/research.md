# Research notes: DFR0991 protocol and ESPHome mapping

This document separates what is **verified** (with source), what is **assumed**, and what is
**open**. The component only relies on verified items; every assumption is called out in the
code where it matters.

Sources:

- **[LIB]** DFRobot_RGBButton Arduino library V1.0.0, `DFRobot_RGBButton.h` / `.cpp`,
  commit `ce7742a` (2022-10-27) – <https://github.com/DFRobot/DFRobot_RGBButton>
- **[PY]** The Raspberry Pi variant in the same repository, `python/raspberrypi/DFRobot_RGBButton.py`
- **[EX]** The library's examples (`getButtonStatus`, `buttonInterrupt`, `breatheLight`, `buttonCascade`)
- **[README]** The library's `README.md`
- **[WIKI]** Product wiki – <https://wiki.dfrobot.com/dfr0991/> (general product facts only; it
  contains no register map)
- **[ESPHOME]** ESPHome 2026.10.2 source code – <https://github.com/esphome/esphome>

## Register map (verified)

These seven registers are all the vendor library defines. No other register is used.

| Register | Name in [LIB] | Access used by [LIB] | Meaning |
|:--:|---|---|---|
| `0x00` | `RGBBUTTON_I2C_ADDR_REG` | read (protected `getI2CAddr()`, via general-call address 0) | Current I2C address of the module |
| `0x01` | `RGBBUTTON_RED_REG` | write | Red, 0–255 |
| `0x02` | `RGBBUTTON_GREEN_REG` | write | Green, 0–255 |
| `0x03` | `RGBBUTTON_BLUE_REG` | write | Blue, 0–255 |
| `0x04` | `RGBBUTTON_BUTTON_SIGNAL_REG` | read | Button state; [LIB] treats any non-zero value as pressed, [PY] compares with `1` |
| `0x09` | `RGBBUTTON_PID_MSB_REG` | read | Product ID, high byte |
| `0x0A` | `RGBBUTTON_PID_LSB_REG` | read | Product ID, low byte |

Other verified facts:

| Fact | Value | Source |
|---|---|---|
| Product ID | `0x43DF` (`RGBBUTTON_PART_ID`), checked in `begin()` | [LIB], [PY] |
| Library default address | `0x2A` (`RGBBUTTON_DEFAULT_I2C_ADDR`) | [LIB], [PY] |
| DIP switch addresses | A2 A1 A0 = `000` → `0x23` … `111` → `0x2A` | [EX] (comment table in every example) |
| DIP change needs a power cycle | "the setting takes effect after encoder power fail and restart" | [EX] |
| Write sequence | START, address+W, register, data bytes…, STOP. Colour is written as one 3-byte write starting at `0x01` | [LIB] `writeReg()`, `setRGBColor()` |
| Read sequence | START, address+W, register, **STOP**; then START, address+R, N bytes, STOP | [LIB] `readReg()` (`endTransmission()` without argument sends a STOP) |
| Bus speed | 100 kHz, set before every transfer (`setClock(100000)`) | [LIB] |
| INT pin | Low while released, high while pressed; the example uses a `CHANGE` interrupt | [README], [EX] `buttonInterrupt` |
| Named colours | red `FF0000`, orange `FF7F00`, yellow `FFFF00`, green `00FF00`, cyan `00FFFF`, blue `0000FF`, purple `8B00FF`, white `FFFFFF`, black `000000` | [LIB] `eGeneralRGBValue_t` |
| Supply, connectors, max. 8 modules | 3.3–5 V; PH2.0-4P and 2.54 mm 5-pin header | [WIKI] |

## What the module does *not* have (verified by absence in [LIB])

| Feature | Finding | Consequence in this component |
|---|---|---|
| Brightness register | None. [README] mentions "brightness by configuring PWM", but the library only ever writes the three colour bytes | Brightness = scaled RGB values (ESPHome light) |
| Breathing / effects | None. The `breatheLight` example computes colours on the host and writes them every 100 ms | Software effects `dfrobot_breathe`, `dfrobot_color_cycle` |
| Click / double click / long press | None. Only the momentary state (register `0x04`) and the INT level | Software state machine in the hub |
| Interrupt configuration / status register | None. INT simply follows the button | ISR sets a flag, state is read via register `0x04` |

## Module function → register → ESPHome

| Module function | Register(s) | ESPHome mapping |
|---|---|---|
| Device identification | read `0x09`–`0x0A` == `0x43DF` | `setup()` and reconnect probe; `connected` binary sensor; `on_connect` / `on_disconnect` |
| Set LED colour | write `0x01`–`0x03` | `light` platform (RGB), plus status / flash / press feedback / boot animation layers |
| Read button | read `0x04` | `button` binary sensor; triggers; click detection; statistics |
| INT pin | – (GPIO) | optional `interrupt_pin` |
| Address selection | DIP switch | `address` with validation `0x23`–`0x2A` |
| Read address register | `0x00` | not used (see open questions) |

## Assumptions

| # | Assumption | Why | Where it matters |
|:--:|---|---|---|
| A1 | DIP switch "1" means the switch is in the ON position | [EX] only lists bit values | README address table. Use the I2C scan if in doubt |
| A2 | Separate write and read transactions are required | Only this sequence is proven by [LIB]; repeated START is undocumented | `read_register_()` deliberately avoids `I2CDevice::read_register()` |
| A3 | The LED is off after the module powers up | Not documented | After a reconnect the current colour is always written again |
| A4 | The firmware may or may not debounce the contact | Not documented | Sampling period > bounce time in polling mode; `debounce` 10 ms in interrupt mode |
| A5 | Registers `0x01`–`0x03` are write-only | [LIB] never reads them | The component keeps its own copy and never reads back |
| A6 | 100 kHz is the safe bus speed | [LIB] forces 100 kHz | Examples set `frequency: 100kHz` |
| A7 | Pin order and cable colours of the connectors; INT being the fifth pin of the 2.54 mm header | [WIKI] could not be re-checked while writing this | README tables go by signal name; follow the board's silkscreen |

## Open questions (need hardware)

1. Does the module accept a repeated START (would halve the number of transactions per poll)?
2. Does it work reliably at 400 kHz?
3. Is register `0x00` readable at the module's own address (the library uses the general-call address)?
4. Are the colour registers readable (would allow verifying the LED state after glitches)?
5. How long does the module need after power-up before it answers?

## ESPHome APIs used (checked against [ESPHOME] 2026.10.2)

| Purpose | API |
|---|---|
| I2C | `i2c::I2CDevice::write()`, `read()`, `write_register()`; `i2c.i2c_device_schema()`, `i2c.register_i2c_device()` |
| Light | `light::LightOutput` (`get_traits`, `setup_state`, `write_state`), `light.RGB_LIGHT_SCHEMA`, `light.register_light()`, `register_rgb_effect()` |
| Entities | `binary_sensor.new_binary_sensor()`, `sensor.new_sensor()`, `text_sensor.new_text_sensor()`, `event.new_event()`, `select.new_select()` + `Select::control(size_t)`, `switch.new_switch()`, `number.new_number()` |
| Automations | `automation.register_action(..., synchronous=True)`, `register_condition()`, `build_automation()`, `TEMPLATABLE_VALUE`, `cg.templatable()` |
| GPIO interrupt | `InternalGPIOPin::attach_interrupt()` with `gpio::INTERRUPT_ANY_EDGE`, ISR marked `IRAM_ATTR` |
| Persistence | `global_preferences->make_preference<T>(key)` with a key derived from the hub ID |
| Platform limits | `i2c` is restricted with `cv.only_on([ESP32, ESP8266, RP2, NRF52, HOST])` → no LibreTiny |

`synchronous=` on `register_action()` exists since ESPHome 2026.3.0, which sets the minimum version.
