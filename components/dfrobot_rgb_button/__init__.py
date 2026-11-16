"""ESPHome external component for the DFRobot Gravity I2C RGB LED Button (DFR0991)."""

import re
import zlib

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BLUE,
    CONF_COLOR,
    CONF_COUNT,
    CONF_DURATION,
    CONF_EFFECT,
    CONF_GAMMA_CORRECT,
    CONF_GREEN,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_MAX_BRIGHTNESS,
    CONF_NAME,
    CONF_ON_CLICK,
    CONF_ON_DOUBLE_CLICK,
    CONF_ON_PRESS,
    CONF_ON_RELEASE,
    CONF_PERIOD,
    CONF_RED,
    CONF_STATUS,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@ShadyMCShadesen"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_DFROBOT_RGB_BUTTON_ID = "dfrobot_rgb_button_id"

CONF_BOOT_ANIMATION = "boot_animation"
CONF_COLOR_CORRECT = "color_correct"
CONF_CONNECTION_STATUS = "connection_status"
CONF_DEBOUNCE = "debounce"
CONF_DOUBLE_CLICK_TIME = "double_click_time"
CONF_LED_REFRESH_INTERVAL = "led_refresh_interval"
CONF_LOCKED = "locked"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_ON_CONNECT = "on_connect"
CONF_ON_DISCONNECT = "on_disconnect"
CONF_ON_LOCKED_PRESS = "on_locked_press"
CONF_ON_LONG_PRESS = "on_long_press"
CONF_PAUSE = "pause"
CONF_PRESS_FEEDBACK = "press_feedback"
CONF_STATUSES = "statuses"
CONF_SYNC_WITH = "sync_with"

dfrobot_rgb_button_ns = cg.esphome_ns.namespace("dfrobot_rgb_button")
DFRobotRGBButton = dfrobot_rgb_button_ns.class_(
    "DFRobotRGBButton", cg.Component, i2c.I2CDevice
)

StatusEffect = dfrobot_rgb_button_ns.enum("StatusEffect")
STATUS_EFFECTS = {
    "solid": StatusEffect.STATUS_EFFECT_SOLID,
    "blink": StatusEffect.STATUS_EFFECT_BLINK,
    "breathe": StatusEffect.STATUS_EFFECT_BREATHE,
}

ConnectionStatusMode = dfrobot_rgb_button_ns.enum("ConnectionStatusMode")
CONNECTION_STATUS_MODES = {
    "none": ConnectionStatusMode.CONNECTION_STATUS_NONE,
    "network": ConnectionStatusMode.CONNECTION_STATUS_NETWORK,
    "api": ConnectionStatusMode.CONNECTION_STATUS_API,
}

FlashAction = dfrobot_rgb_button_ns.class_("FlashAction", automation.Action)
StopFlashAction = dfrobot_rgb_button_ns.class_("StopFlashAction", automation.Action)
SetStatusAction = dfrobot_rgb_button_ns.class_("SetStatusAction", automation.Action)
SetMaxBrightnessAction = dfrobot_rgb_button_ns.class_(
    "SetMaxBrightnessAction", automation.Action
)
LockAction = dfrobot_rgb_button_ns.class_("LockAction", automation.Action)
UnlockAction = dfrobot_rgb_button_ns.class_("UnlockAction", automation.Action)
ResetPressCountAction = dfrobot_rgb_button_ns.class_(
    "ResetPressCountAction", automation.Action
)
IsPressedCondition = dfrobot_rgb_button_ns.class_(
    "IsPressedCondition", automation.Condition
)
IsLockedCondition = dfrobot_rgb_button_ns.class_(
    "IsLockedCondition", automation.Condition
)
IsConnectedCondition = dfrobot_rgb_button_ns.class_(
    "IsConnectedCondition", automation.Condition
)

# ---------------------------------------------------------------------------
# Facts taken from the vendor library (github.com/DFRobot/DFRobot_RGBButton).
# ---------------------------------------------------------------------------

# DIP switch A2..A0 = 000 -> 0x23 ... 111 -> 0x2A (table in the library examples).
ADDRESS_MIN = 0x23
ADDRESS_MAX = 0x2A
# RGBBUTTON_DEFAULT_I2C_ADDR in DFRobot_RGBButton.h.
DEFAULT_ADDRESS = 0x2A

# eGeneralRGBValue_t in DFRobot_RGBButton.h.
NAMED_COLORS = {
    "red": 0xFF0000,
    "orange": 0xFF7F00,
    "yellow": 0xFFFF00,
    "green": 0x00FF00,
    "cyan": 0x00FFFF,
    "blue": 0x0000FF,
    "purple": 0x8B00FF,
    "white": 0xFFFFFF,
    "black": 0x000000,
}

# ---------------------------------------------------------------------------
# Defaults that are design decisions of this component (change them here).
# ---------------------------------------------------------------------------

DEFAULT_UPDATE_INTERVAL = "20ms"
DEFAULT_DEBOUNCE_POLLING = "0ms"
DEFAULT_DEBOUNCE_INTERRUPT = "10ms"
DEFAULT_LONG_PRESS_TIME = "800ms"
DEFAULT_DOUBLE_CLICK_TIME = "300ms"
DEFAULT_GAMMA_CORRECT = 2.8
STATUS_IDLE = "idle"
STATUS_CONNECTING = "connecting"
MAX_STATUSES = 32

_HEX_COLOR_RE = re.compile(r"^#?([0-9a-fA-F]{6})$")


def _rgb_from_int(value: int) -> dict[str, float]:
    return {
        CONF_RED: ((value >> 16) & 0xFF) / 255.0,
        CONF_GREEN: ((value >> 8) & 0xFF) / 255.0,
        CONF_BLUE: (value & 0xFF) / 255.0,
    }


_RGB_DICT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RED, default=0.0): cv.percentage,
        cv.Optional(CONF_GREEN, default=0.0): cv.percentage,
        cv.Optional(CONF_BLUE, default=0.0): cv.percentage,
    }
)


def color(value) -> dict[str, float]:
    """Validate a color.

    Accepted: a name from the vendor library (``red``, ``orange``, ``yellow``,
    ``green``, ``cyan``, ``blue``, ``purple``, ``white``, ``black``), a hex string
    (``"#FF8000"``), an integer (``0xFF8000``) or a mapping with ``red``,
    ``green`` and ``blue`` percentages.
    """
    if isinstance(value, dict):
        return _RGB_DICT_SCHEMA(value)
    if isinstance(value, bool):
        raise cv.Invalid("Expected a color, not a boolean")
    if isinstance(value, int):
        if not 0 <= value <= 0xFFFFFF:
            raise cv.Invalid("Integer colors must be in the range 0x000000-0xFFFFFF")
        return _rgb_from_int(value)
    text = cv.string(value).strip()
    if text.lower() in NAMED_COLORS:
        return _rgb_from_int(NAMED_COLORS[text.lower()])
    if match := _HEX_COLOR_RE.match(text):
        return _rgb_from_int(int(match.group(1), 16))
    raise cv.Invalid(
        f"'{value}' is not a valid color. Use one of {', '.join(NAMED_COLORS)}, "
        'a quoted hex string such as "#FF8000", or a mapping with red/green/blue'
    )


def _press_feedback(value):
    if isinstance(value, bool):
        return color("white") if value else None
    return color(value)


def _validate_address(value):
    value = cv.i2c_address(value)
    if not ADDRESS_MIN <= value <= ADDRESS_MAX:
        raise cv.Invalid(
            f"Address 0x{value:02X} cannot be set with the DIP switch. "
            f"Valid addresses are 0x{ADDRESS_MIN:02X} (A2..A0 = 000) to "
            f"0x{ADDRESS_MAX:02X} (A2..A0 = 111)"
        )
    return value


DEFAULT_STATUSES = [
    {
        CONF_NAME: "ok",
        CONF_COLOR: "green",
        CONF_EFFECT: "solid",
        CONF_PERIOD: "1s",
    },
    {
        CONF_NAME: "warning",
        CONF_COLOR: "orange",
        CONF_EFFECT: "blink",
        CONF_PERIOD: "1s",
    },
    {
        CONF_NAME: "error",
        CONF_COLOR: "red",
        CONF_EFFECT: "blink",
        CONF_PERIOD: "400ms",
    },
    {
        CONF_NAME: STATUS_CONNECTING,
        CONF_COLOR: "blue",
        CONF_EFFECT: "breathe",
        CONF_PERIOD: "2s",
    },
]

STATUS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Required(CONF_COLOR): color,
        cv.Optional(CONF_EFFECT, default="solid"): cv.enum(STATUS_EFFECTS, lower=True),
        cv.Optional(CONF_PERIOD, default="1s"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=100)),
        ),
    }
)


def _merge_statuses(config: ConfigType) -> ConfigType:
    """Build the final status list: idle, the defaults, then user overrides/additions."""
    merged: dict[str, dict] = {
        entry[CONF_NAME]: STATUS_SCHEMA(entry) for entry in DEFAULT_STATUSES
    }
    seen = set()
    for index, entry in enumerate(config.get(CONF_STATUSES, [])):
        name = entry[CONF_NAME]
        if name == STATUS_IDLE:
            raise cv.Invalid(
                f"'{STATUS_IDLE}' is reserved: it shows the light and cannot be redefined",
                path=[CONF_STATUSES, index, CONF_NAME],
            )
        if name in seen:
            raise cv.Invalid(
                f"Status '{name}' is defined twice",
                path=[CONF_STATUSES, index, CONF_NAME],
            )
        seen.add(name)
        merged[name] = entry
    if len(merged) + 1 > MAX_STATUSES:
        raise cv.Invalid(
            f"At most {MAX_STATUSES} statuses are supported", path=[CONF_STATUSES]
        )
    config = config.copy()
    config[CONF_STATUSES] = list(merged.values())
    return config


def _validate_config(config: ConfigType) -> ConfigType:
    if CONF_DEBOUNCE not in config:
        config = config.copy()
        default = (
            DEFAULT_DEBOUNCE_INTERRUPT
            if CONF_INTERRUPT_PIN in config
            else DEFAULT_DEBOUNCE_POLLING
        )
        config[CONF_DEBOUNCE] = cv.positive_time_period_milliseconds(default)
    if CONF_SYNC_WITH in config and config[CONF_SYNC_WITH] == config[CONF_ID]:
        raise cv.Invalid("A module cannot be synced with itself", path=[CONF_SYNC_WITH])
    return config


def status_names(config: ConfigType) -> list[str]:
    """Names of all statuses of a validated hub config, index 0 being idle."""
    return [STATUS_IDLE] + [entry[CONF_NAME] for entry in config[CONF_STATUSES]]


def hub_config(hub_id: ID) -> ConfigType:
    """Return the validated configuration of the hub with the given ID."""
    from esphome.core import CORE

    for conf in CORE.config.get("dfrobot_rgb_button", []):
        if conf[CONF_ID] == hub_id:
            return conf
    raise cv.Invalid(f"No dfrobot_rgb_button with id '{hub_id}' found")


def storage_key(hub_id: ID, purpose: str) -> int:
    """Stable 32-bit key for values persisted in flash, derived from the hub ID."""
    return zlib.crc32(f"dfrobot_rgb_button/{hub_id.id}/{purpose}".encode())


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DFRobotRGBButton),
            cv.Optional(CONF_UPDATE_INTERVAL, default=DEFAULT_UPDATE_INTERVAL): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=1),
                    max=cv.TimePeriod(milliseconds=1000),
                ),
            ),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_DEBOUNCE): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=500)),
            ),
            cv.Optional(CONF_LONG_PRESS_TIME, default=DEFAULT_LONG_PRESS_TIME): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=100)),
            ),
            cv.Optional(
                CONF_DOUBLE_CLICK_TIME, default=DEFAULT_DOUBLE_CLICK_TIME
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PRESS_FEEDBACK): _press_feedback,
            cv.Optional(CONF_MAX_BRIGHTNESS, default="100%"): cv.percentage,
            cv.Optional(CONF_COLOR_CORRECT): cv.All(
                [cv.percentage], cv.Length(min=3, max=3)
            ),
            cv.Optional(
                CONF_GAMMA_CORRECT, default=DEFAULT_GAMMA_CORRECT
            ): cv.positive_float,
            cv.Optional(CONF_STATUSES): cv.ensure_list(STATUS_SCHEMA),
            cv.Optional(CONF_CONNECTION_STATUS, default="none"): cv.enum(
                CONNECTION_STATUS_MODES, lower=True
            ),
            cv.Optional(CONF_SYNC_WITH): cv.use_id(DFRobotRGBButton),
            cv.Optional(CONF_BOOT_ANIMATION, default=False): cv.boolean,
            cv.Optional(CONF_LED_REFRESH_INTERVAL): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=100)),
            ),
            cv.Optional(CONF_LOCKED, default=False): cv.boolean,
            cv.Optional(CONF_ON_PRESS): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_RELEASE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_CLICK): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_DOUBLE_CLICK): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_LONG_PRESS): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_LOCKED_PRESS): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                single=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS))
    .extend({cv.Optional(CONF_ADDRESS, default=DEFAULT_ADDRESS): _validate_address}),
    _merge_statuses,
    _validate_config,
)


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    mode = config[CONF_CONNECTION_STATUS]
    if mode == "api" and "api" not in full_config:
        raise cv.Invalid(
            "'connection_status: api' requires the 'api' component",
            path=[CONF_CONNECTION_STATUS],
        )
    if mode == "network" and "network" not in full_config:
        raise cv.Invalid(
            "'connection_status: network' requires a network component such as 'wifi' or 'ethernet'",
            path=[CONF_CONNECTION_STATUS],
        )
    # Two modules on the same bus must not share an address.
    seen = set()
    for conf in full_config.get("dfrobot_rgb_button", []):
        key = (str(conf.get(i2c.CONF_I2C_ID)), conf[CONF_ADDRESS])
        if key in seen:
            raise cv.Invalid(
                f"Address 0x{conf[CONF_ADDRESS]:02X} is used by more than one "
                "dfrobot_rgb_button on the same I2C bus; give every module its own "
                "DIP switch setting",
                path=[CONF_ADDRESS],
            )
        seen.add(key)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate

_TRIGGERS = (
    (CONF_ON_PRESS, "get_press_trigger", []),
    (CONF_ON_RELEASE, "get_release_trigger", [(cg.uint32, "duration")]),
    (CONF_ON_CLICK, "get_click_trigger", []),
    (CONF_ON_DOUBLE_CLICK, "get_double_click_trigger", []),
    (CONF_ON_LONG_PRESS, "get_long_press_trigger", []),
    (CONF_ON_LOCKED_PRESS, "get_locked_press_trigger", []),
    (CONF_ON_CONNECT, "get_connect_trigger", []),
    (CONF_ON_DISCONNECT, "get_disconnect_trigger", []),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_debounce(config[CONF_DEBOUNCE]))
    cg.add(var.set_long_press_time(config[CONF_LONG_PRESS_TIME]))
    cg.add(var.set_double_click_time(config[CONF_DOUBLE_CLICK_TIME]))
    cg.add(var.set_gamma_correct(config[CONF_GAMMA_CORRECT]))
    cg.add(var.set_max_brightness(config[CONF_MAX_BRIGHTNESS]))

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        pin = await cg.gpio_pin_expression(interrupt_pin)
        cg.add(var.set_interrupt_pin(pin))
    if (feedback := config.get(CONF_PRESS_FEEDBACK)) is not None:
        cg.add(
            var.set_press_feedback(
                feedback[CONF_RED], feedback[CONF_GREEN], feedback[CONF_BLUE]
            )
        )
    if color_correct := config.get(CONF_COLOR_CORRECT):
        cg.add(var.set_color_correct(*color_correct))
    if config[CONF_BOOT_ANIMATION]:
        cg.add(var.set_boot_animation(True))
    if CONF_LED_REFRESH_INTERVAL in config:
        cg.add(var.set_led_refresh_interval(config[CONF_LED_REFRESH_INTERVAL]))
    if config[CONF_LOCKED]:
        cg.add(var.set_locked(True))

    # Index 0 is always "idle" (transparent: the light shows through).
    cg.add(var.add_status(STATUS_IDLE, 0.0, 0.0, 0.0, STATUS_EFFECTS["solid"], 0))
    for status in config[CONF_STATUSES]:
        rgb = status[CONF_COLOR]
        cg.add(
            var.add_status(
                status[CONF_NAME],
                rgb[CONF_RED],
                rgb[CONF_GREEN],
                rgb[CONF_BLUE],
                status[CONF_EFFECT],
                status[CONF_PERIOD],
            )
        )
    if config[CONF_CONNECTION_STATUS] != "none":
        cg.add(
            var.set_connection_status(
                config[CONF_CONNECTION_STATUS],
                status_names(config).index(STATUS_CONNECTING),
            )
        )

    if CONF_SYNC_WITH in config:
        source = await cg.get_variable(config[CONF_SYNC_WITH])
        cg.add(var.set_sync_source(source))

    for conf_key, getter, args in _TRIGGERS:
        if conf_key in config:
            await automation.build_automation(
                getattr(var, getter)(), args, config[conf_key]
            )


# ---------------------------------------------------------------------------
# Actions and conditions
# ---------------------------------------------------------------------------

_SIMPLE_ID_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(DFRobotRGBButton)}
)


def _expand_flash_color(config: ConfigType) -> ConfigType:
    """Turn the static ``color`` shorthand into red/green/blue values."""
    has_channels = any(key in config for key in (CONF_RED, CONF_GREEN, CONF_BLUE))
    if CONF_COLOR in config and has_channels:
        raise cv.Invalid("Use either 'color' or 'red'/'green'/'blue', not both")
    config = config.copy()
    if CONF_COLOR in config:
        config.update(config.pop(CONF_COLOR))
    elif not has_channels:
        config.update(color("white"))
    for key in (CONF_RED, CONF_GREEN, CONF_BLUE):
        config.setdefault(key, 0.0)
    return config


FLASH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DFRobotRGBButton),
            cv.Optional(CONF_COLOR): color,
            cv.Optional(CONF_RED): cv.templatable(cv.percentage),
            cv.Optional(CONF_GREEN): cv.templatable(cv.percentage),
            cv.Optional(CONF_BLUE): cv.templatable(cv.percentage),
            cv.Optional(CONF_COUNT, default=3): cv.templatable(
                cv.int_range(min=1, max=1000)
            ),
            cv.Optional(CONF_DURATION, default="200ms"): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
            cv.Optional(CONF_PAUSE): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
    _expand_flash_color,
)


@automation.register_action(
    "dfrobot_rgb_button.flash", FlashAction, FLASH_ACTION_SCHEMA, synchronous=True
)
async def flash_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    for key in (CONF_RED, CONF_GREEN, CONF_BLUE):
        value = await cg.templatable(config[key], args, cg.float_)
        cg.add(getattr(var, f"set_{key}")(value))
    cg.add(var.set_count(await cg.templatable(config[CONF_COUNT], args, cg.uint32)))
    duration = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
    cg.add(var.set_duration(duration))
    # The pause defaults to the on-time, giving a symmetric blink.
    pause = await cg.templatable(
        config.get(CONF_PAUSE, config[CONF_DURATION]), args, cg.uint32
    )
    cg.add(var.set_pause(pause))
    return var


@automation.register_action(
    "dfrobot_rgb_button.set_status",
    SetStatusAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DFRobotRGBButton),
            cv.Required(CONF_STATUS): cv.templatable(cv.string_strict),
            cv.Optional(CONF_DURATION, default="0ms"): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
    synchronous=True,
)
async def set_status_action_to_code(config, action_id, template_arg, args):
    status = config[CONF_STATUS]
    if not cg.is_template(status):
        names = status_names(hub_config(config[CONF_ID]))
        if status not in names:
            raise cv.Invalid(
                f"Unknown status '{status}' in dfrobot_rgb_button.set_status. "
                f"Defined statuses: {', '.join(names)}"
            )
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_status(await cg.templatable(status, args, cg.std_string)))
    duration = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
    cg.add(var.set_duration(duration))
    return var


@automation.register_action(
    "dfrobot_rgb_button.set_max_brightness",
    SetMaxBrightnessAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(DFRobotRGBButton),
            cv.Required(CONF_MAX_BRIGHTNESS): cv.templatable(cv.percentage),
        },
        key=CONF_MAX_BRIGHTNESS,
    ),
    synchronous=True,
)
async def set_max_brightness_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    value = await cg.templatable(config[CONF_MAX_BRIGHTNESS], args, cg.float_)
    cg.add(var.set_max_brightness(value))
    return var


@automation.register_action(
    "dfrobot_rgb_button.stop_flash",
    StopFlashAction,
    _SIMPLE_ID_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "dfrobot_rgb_button.lock", LockAction, _SIMPLE_ID_SCHEMA, synchronous=True
)
@automation.register_action(
    "dfrobot_rgb_button.unlock", UnlockAction, _SIMPLE_ID_SCHEMA, synchronous=True
)
@automation.register_action(
    "dfrobot_rgb_button.reset_press_count",
    ResetPressCountAction,
    _SIMPLE_ID_SCHEMA,
    synchronous=True,
)
async def simple_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_condition(
    "dfrobot_rgb_button.is_pressed", IsPressedCondition, _SIMPLE_ID_SCHEMA
)
@automation.register_condition(
    "dfrobot_rgb_button.is_locked", IsLockedCondition, _SIMPLE_ID_SCHEMA
)
@automation.register_condition(
    "dfrobot_rgb_button.is_connected", IsConnectedCondition, _SIMPLE_ID_SCHEMA
)
async def simple_condition_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
