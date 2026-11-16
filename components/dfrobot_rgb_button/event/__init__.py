import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_BUTTON
from esphome.types import ConfigType

from .. import CONF_DFROBOT_RGB_BUTTON_ID, DFRobotRGBButton

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_CLICK = "click"

# Must match click_type_to_string() in dfrobot_rgb_button.cpp.
CLICK_EVENT_TYPES = ["single", "double", "long"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_CLICK): event.event_schema(device_class=DEVICE_CLASS_BUTTON),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_CLICK):
        var = await event.new_event(conf, event_types=CLICK_EVENT_TYPES)
        cg.add(hub.set_click_event(var))
