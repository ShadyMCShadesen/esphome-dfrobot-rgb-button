import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_DFROBOT_RGB_BUTTON_ID, DFRobotRGBButton, dfrobot_rgb_button_ns

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_LOCK = "lock"

LockSwitch = dfrobot_rgb_button_ns.class_("LockSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_LOCK): switch.switch_schema(
            LockSwitch,
            icon="mdi:lock",
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_LOCK):
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(hub.set_lock_switch(var))
