import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_RESTORE,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_COUNTER,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_MILLISECOND,
)
from esphome.types import ConfigType

from .. import CONF_DFROBOT_RGB_BUTTON_ID, DFRobotRGBButton, storage_key

DEPENDENCIES = ["dfrobot_rgb_button"]

CONF_LAST_PRESS_DURATION = "last_press_duration"
CONF_PRESS_COUNT = "press_count"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_RGB_BUTTON_ID): cv.use_id(DFRobotRGBButton),
        cv.Optional(CONF_PRESS_COUNT): sensor.sensor_schema(
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend({cv.Optional(CONF_RESTORE, default=False): cv.boolean}),
        cv.Optional(CONF_LAST_PRESS_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLISECOND,
            icon=ICON_TIMER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_RGB_BUTTON_ID])
    if conf := config.get(CONF_PRESS_COUNT):
        sens = await sensor.new_sensor(conf)
        cg.add(hub.set_press_count_sensor(sens))
        if conf[CONF_RESTORE]:
            key = storage_key(config[CONF_DFROBOT_RGB_BUTTON_ID], "press_count")
            cg.add(hub.set_press_count_restore_key(key))
    if conf := config.get(CONF_LAST_PRESS_DURATION):
        sens = await sensor.new_sensor(conf)
        cg.add(hub.set_last_press_duration_sensor(sens))
