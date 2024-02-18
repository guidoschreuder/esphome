import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SENSORS

from .. import EbusComponent, CONF_EBUS_ID, CONF_MASTER_ADDRESS, ebus_ns

AUTO_LOAD = ["ebus"]

EbusSensor = ebus_ns.class_("EbusSensor", sensor.Sensor, cg.Component)

CONF_SENSOR_MESSAGE = "message"
CONF_SENSOR_MESSAGE_SEND_POLL = "send_poll"
CONF_SENSOR_MESSAGE_SOURCE = "source"
CONF_SENSOR_MESSAGE_DESTINATION = "destination"
CONF_SENSOR_MESSAGE_COMMAND = "command"
CONF_SENSOR_MESSAGE_PAYLOAD = "payload"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EbusSensor),
        cv.GenerateID(CONF_EBUS_ID): cv.use_id(EbusComponent),
        cv.Required(CONF_SENSORS): cv.ensure_list(sensor.sensor_schema().extend(
            {
                cv.GenerateID(): cv.declare_id(EbusSensor),
                cv.Required(CONF_SENSOR_MESSAGE): cv.Schema(
                    {
                        cv.Optional(CONF_SENSOR_MESSAGE_SEND_POLL, False): cv.boolean,
                        cv.Optional(CONF_SENSOR_MESSAGE_SOURCE, 0XAA): cv.hex_uint8_t,
                        cv.Optional(CONF_SENSOR_MESSAGE_DESTINATION, 0XAA): cv.hex_uint8_t,
                        cv.Required(CONF_SENSOR_MESSAGE_COMMAND): cv.hex_uint16_t,
                        cv.Required(CONF_SENSOR_MESSAGE_PAYLOAD): cv.Schema([cv.hex_uint8_t]),
                    }
                ),
            }
        )),
    }
)

async def to_code(config):
    ebus = await cg.get_variable(config[CONF_EBUS_ID])


    for i, conf in enumerate(config[CONF_SENSORS]):
        print(f"Sensor: {i}, {conf}")
        sens = await sensor.new_sensor(conf)
        cg.add(sens.set_send_poll(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_SEND_POLL]))
        cg.add(sens.set_source(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_SOURCE]))
        cg.add(sens.set_destination(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_DESTINATION]))
        cg.add(sens.set_command(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_COMMAND]))
        cg.add(sens.set_payload(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_PAYLOAD]))
        cg.add(ebus.add_receiver(sens))
        if sens.set_send_poll(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_SEND_POLL]):
            cg.add(ebus.add_sender(sens))


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
