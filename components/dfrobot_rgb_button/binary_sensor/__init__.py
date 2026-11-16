import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUTTON,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from .. import CONF_DFROBOT_RGB_BUTTON_ID, DFRobotRGBButton

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_CONNECTED = "connected"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_BUTTON):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.set_button_binary_sensor(sens))
    if conf := config.get(CONF_CONNECTED):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.set_connected_binary_sensor(sens))
