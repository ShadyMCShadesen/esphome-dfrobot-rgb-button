import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_BRIGHTNESS,
    CONF_RESTORE_VALUE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import (
    CONF_DFROBOT_RGB_BUTTON_ID,
    DFRobotRGBButton,
    dfrobot_rgb_button_ns,
    storage_key,
)

DEPENDENCIES = ["dfrobot_rgb_button"]

MaxBrightnessNumber = dfrobot_rgb_button_ns.class_(
    "MaxBrightnessNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_MAX_BRIGHTNESS): number.number_schema(
            MaxBrightnessNumber,
            icon="mdi:brightness-6",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERCENT,
        )
        .extend({cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean})
        .extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_MAX_BRIGHTNESS):
        var = await number.new_number(conf, min_value=0, max_value=100, step=1)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        if conf[CONF_RESTORE_VALUE]:
            key = storage_key(config[CONF_DFROBOT_RGB_BUTTON_ID], "max_brightness")
            cg.add(var.set_restore_key(key))
        cg.add(hub.set_max_brightness_number(var))
