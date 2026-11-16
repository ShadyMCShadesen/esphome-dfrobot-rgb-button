# Wiring

The DFR0991 has two connectors that carry the same signals: a PH2.0-4P "Gravity" socket
(VCC, GND, SCL, SDA) and a 2.54 mm 5-pin header that additionally exposes **INT** (the fifth
pin; the PH2.0 socket has only four contacts).

> Go by the labels printed on your board. The tables below are by signal name; the physical pin
> order is not repeated here because it could not be verified against the vendor wiki while
> writing this document.

## Signals

| Signal | Connect to | Required |
|---|---|:--:|
| VCC | 3.3 V (or 5 V on 5 V systems) | yes |
| GND | GND | yes |
| SDA | I2C data pin of the ESP | yes |
| SCL | I2C clock pin of the ESP | yes |
| INT | Any interrupt-capable GPIO | **no** – only for `interrupt_pin:` |

Power the module from **3.3 V** when it is connected to an ESP32, ESP8266 or RP2040. The module
accepts 3.3–5 V, but if it pulls SDA/SCL up to its own supply voltage, a 5 V supply would put
5 V on pins that these microcontrollers do not tolerate.

Gravity I2C cables normally use red = VCC, black = GND, blue = SCL, green = SDA. Check before
powering up.

## Typical pins

| Board | SDA | SCL | INT (optional, example) |
|---|---|---|---|
| ESP32 DevKit | GPIO21 | GPIO22 | GPIO4 |
| ESP32-C3 DevKitM | GPIO8 | GPIO9 | GPIO4 |
| ESP32-S3 DevKitC | GPIO8 | GPIO9 | GPIO4 |
| ESP32-C6 DevKitC | GPIO6 | GPIO7 | GPIO4 |
| ESP8266 D1 mini | GPIO4 (D2) | GPIO5 (D1) | GPIO14 (D5) |
| Raspberry Pi Pico W | GPIO4 | GPIO5 | GPIO6 |

Any other I2C-capable pins work as well; set them under `i2c:`.

## One module, polling (default)

```text
 ESP                    DFR0991
 3V3  ───────────────── VCC
 GND  ───────────────── GND
 SDA  ───────────────── SDA
 SCL  ───────────────── SCL
                        INT   (not connected)
```

## One module with INT (optional)

```text
 ESP                    DFR0991
 3V3  ───────────────── VCC
 GND  ───────────────── GND
 SDA  ───────────────── SDA
 SCL  ───────────────── SCL
 GPIO ───────────────── INT   (high while pressed)
```

## Several modules

All modules are wired in parallel. Set a different DIP switch address on each one and
power-cycle it afterwards.

```text
 ESP          #1 (0x23)     #2 (0x24)     #3 (0x25)
 3V3  ──────── VCC ───────── VCC ───────── VCC
 GND  ──────── GND ───────── GND ───────── GND
 SDA  ──────── SDA ───────── SDA ───────── SDA
 SCL  ──────── SCL ───────── SCL ───────── SCL
```

With `interrupt_pin`, every module needs its own GPIO. INT is high while the button is pressed;
the output type (push-pull or open-drain) is not documented, so do not tie several INT outputs
together. Polling needs no extra wires, which is why it is the default.

## Cable length

I2C is meant for short connections. As a rule of thumb keep the total bus length below about one
metre at 100 kHz. For longer runs lower the speed (`frequency: 50kHz` or `10kHz`), use twisted
pairs (SDA+GND, SCL+GND) and consider stronger pull-ups (e.g. 2.2 kΩ to 3.3 V). The
`connected` binary sensor and the `on_disconnect` trigger make marginal wiring visible.
