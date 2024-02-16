import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SENSORS

from .. import EbusComponent, CONF_EBUS_ID

AUTO_LOAD = ["ebus"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EBUS_ID): cv.use_id(EbusComponent),
        cv.Required(CONF_SENSORS): cv.ensure_list(sensor.sensor_schema()),

    }
)

async def to_code(config):
    ebus = await cg.get_variable(config[CONF_EBUS_ID])

    for i, conf in enumerate(config[CONF_SENSORS]):
        print(f"Sensor: {i}, {conf}")
        # sens = cg.Pvariable(conf[CONF_ID], var.get_sensor(i))
        # await sensor.register_sensor(sens, conf)


        # sensors = []
    # for key, conf in config.items():
    #     if not isinstance(conf, dict):
    #         continue
    #     id = conf[CONF_ID]
    #     if id and id.type == sensor.Sensor:
    #         sens = await sensor.new_sensor(conf)
    #         cg.add(getattr(hub, f"set_{key}")(sens))
    #         sensors.append(f"F({key})")
