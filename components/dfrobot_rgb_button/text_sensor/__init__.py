import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .. import CONF_DFROBOT_RGB_BUTTON_ID, DFRobotRGBButton

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_LAST_CLICK = "last_click"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_LAST_CLICK): text_sensor.text_sensor_schema(
            icon="mdi:gesture-tap",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_LAST_CLICK):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(hub.set_last_click_text_sensor(sens))
