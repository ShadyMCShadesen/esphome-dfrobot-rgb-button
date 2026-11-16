import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR,
    CONF_GREEN,
    CONF_NAME,
    CONF_PRESET,
    CONF_RED,
    CONF_STATUS,
    ENTITY_CATEGORY_CONFIG,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_DFROBOT_RGB_BUTTON_ID,
    NAMED_COLORS,
    DFRobotRGBButton,
    color,
    dfrobot_rgb_button_ns,
    hub_config,
    status_names,
)

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_PRESETS = "presets"

StatusSelect = dfrobot_rgb_button_ns.class_("StatusSelect", select.Select)
PresetSelect = dfrobot_rgb_button_ns.class_("PresetSelect", select.Select)

# Without an explicit list the named colors of the vendor library are offered
# (black is left out: it equals turning the light off).
DEFAULT_PRESETS = [
    {CONF_NAME: name.capitalize(), CONF_COLOR: name}
    for name in NAMED_COLORS
    if name != "black"
]

PRESET_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Required(CONF_COLOR): color,
    }
)


def _validate_presets(value):
    value = cv.ensure_list(PRESET_SCHEMA)(value)
    if not value:
        raise cv.Invalid("At least one preset is required")
    names = [preset[CONF_NAME] for preset in value]
    if len(set(names)) != len(names):
        raise cv.Invalid("Preset names must be unique")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_STATUS): select.select_schema(
            StatusSelect,
            icon="mdi:led-on",
        ),
        cv.Optional(CONF_PRESET): select.select_schema(
            PresetSelect,
            icon="mdi:palette",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ).extend(
            {cv.Optional(CONF_PRESETS, default=DEFAULT_PRESETS): _validate_presets}
        ),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_PRESET not in config:
        return config
    # Presets are applied to the light entity, so the module needs one.
    hub_id = config[CONF_DFROBOT_RGB_BUTTON_ID]
    for light_conf in fv.full_config.get().get("light", []):
        if light_conf.get(CONF_DFROBOT_RGB_BUTTON_ID) == hub_id:
            return config
    raise cv.Invalid(
        "The 'preset' select changes the color of the light entity. Add a "
        f"'light' with 'platform: dfrobot_rgb_button' for '{hub_id}'",
        path=[CONF_PRESET],
    )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_STATUS):
        options = status_names(hub_config(config[CONF_DFROBOT_RGB_BUTTON_ID]))
        var = await select.new_select(conf, options=options)
        await cg.register_parented(var, hub)
        cg.add(hub.set_status_select(var))
    if conf := config.get(CONF_PRESET):
        presets = conf[CONF_PRESETS]
        var = await select.new_select(
            conf, options=[preset[CONF_NAME] for preset in presets]
        )
        await cg.register_parented(var, hub)
        for preset in presets:
            rgb = preset[CONF_COLOR]
            cg.add(var.add_preset(rgb[CONF_RED], rgb[CONF_GREEN], rgb[CONF_BLUE]))
