import esphome.codegen as cg
from esphome.components import light
from esphome.components.light.effects import register_rgb_effect
from esphome.components.light.types import LightEffect
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_BRIGHTNESS,
    CONF_MIN_BRIGHTNESS,
    CONF_NAME,
    CONF_OUTPUT_ID,
    CONF_PERIOD,
    CONF_UPDATE_INTERVAL,
)
from esphome.types import ConfigType

from .. import CONF_DFROBOT_RGB_BUTTON_ID, DFRobotRGBButton, dfrobot_rgb_button_ns

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_AUTO_OFF = "auto_off"
CONF_TOGGLE_MODE = "toggle_mode"

DFRobotRGBButtonLight = dfrobot_rgb_button_ns.class_(
    "DFRobotRGBButtonLight", light.LightOutput
)
BreatheLightEffect = dfrobot_rgb_button_ns.class_("BreatheLightEffect", LightEffect)
ColorCycleLightEffect = dfrobot_rgb_button_ns.class_(
    "ColorCycleLightEffect", LightEffect
)

ToggleEvent = dfrobot_rgb_button_ns.enum("ToggleEvent")
TOGGLE_EVENTS = {
    "none": ToggleEvent.TOGGLE_EVENT_NONE,
    "press": ToggleEvent.TOGGLE_EVENT_PRESS,
    "release": ToggleEvent.TOGGLE_EVENT_RELEASE,
    "click": ToggleEvent.TOGGLE_EVENT_CLICK,
    "double_click": ToggleEvent.TOGGLE_EVENT_DOUBLE_CLICK,
    "long_press": ToggleEvent.TOGGLE_EVENT_LONG_PRESS,
}


def _toggle_mode(value):
    # `toggle_mode: true` is shorthand for toggling on press.
    if isinstance(value, bool):
        value = "press" if value else "none"
    return cv.enum(TOGGLE_EVENTS, lower=True)(value)


CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DFRobotRGBButtonLight),
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_TOGGLE_MODE, default="none"): _toggle_mode,
        cv.Optional(CONF_AUTO_OFF): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(seconds=1)),
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    cg.add(var.set_parent(hub))
    await light.register_light(var, config)

    if config[CONF_TOGGLE_MODE] != "none":
        cg.add(hub.set_toggle_event(config[CONF_TOGGLE_MODE]))
    if CONF_AUTO_OFF in config:
        cg.add(hub.set_auto_off(config[CONF_AUTO_OFF]))


# ---------------------------------------------------------------------------
# Software effects. The module has no effect engine of its own; these mirror what
# the vendor examples do on the host side and work on any RGB light.
# ---------------------------------------------------------------------------

_EFFECT_INTERVAL = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=15)),
)
_EFFECT_PERIOD = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=200)),
)


def _validate_brightness_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_BRIGHTNESS] >= config[CONF_MAX_BRIGHTNESS]:
        raise cv.Invalid("min_brightness must be lower than max_brightness")
    return config


@register_rgb_effect(
    "dfrobot_breathe",
    BreatheLightEffect,
    "Breathe",
    {
        cv.Optional(CONF_PERIOD, default="3s"): _EFFECT_PERIOD,
        cv.Optional(CONF_MIN_BRIGHTNESS, default="10%"): cv.percentage,
        cv.Optional(CONF_MAX_BRIGHTNESS, default="100%"): cv.percentage,
        cv.Optional(CONF_UPDATE_INTERVAL, default="30ms"): _EFFECT_INTERVAL,
    },
    _validate_brightness_range,
)
async def breathe_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_period(config[CONF_PERIOD]))
    cg.add(effect.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(
        effect.set_min_max_brightness(
            config[CONF_MIN_BRIGHTNESS], config[CONF_MAX_BRIGHTNESS]
        )
    )
    return effect


@register_rgb_effect(
    "dfrobot_color_cycle",
    ColorCycleLightEffect,
    "Color Cycle",
    {
        cv.Optional(CONF_PERIOD, default="18s"): _EFFECT_PERIOD,
        cv.Optional(CONF_UPDATE_INTERVAL, default="50ms"): _EFFECT_INTERVAL,
    },
)
async def color_cycle_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_period(config[CONF_PERIOD]))
    cg.add(effect.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    return effect
