import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SENSORS
import voluptuous as vol

from .. import EbusComponent, CONF_EBUS_ID, CONF_PRIMARY_ADDRESS, ebus_ns

AUTO_LOAD = ["ebus"]

EbusSensor = ebus_ns.class_("EbusSensor", sensor.Sensor, cg.Component)

CONF_SENSOR_MESSAGE = "message"
CONF_SENSOR_MESSAGE_SOURCE = "source"
CONF_SENSOR_MESSAGE_DESTINATION = "destination"
CONF_SENSOR_MESSAGE_COMMAND = "command"
CONF_SENSOR_MESSAGE_PAYLOAD = "payload"
CONF_SENSOR_DECODE = "decode"
CONF_SENSOR_DECODE_POSITION = "position"
CONF_SENSOR_DECODE_BYTES = "bytes"
CONF_SENSOR_DECODE_DIVIDER = "divider"


SYN = 0xAA
ESC = 0xA9

def validate_address(destination):
    if destination == SYN:
        raise vol.Invalid("SYN symbol (0xAA) is not a valid address")
    if destination == ESC:
        raise vol.Invalid("ESC symbol (0xA9) is not a valid address")
    return cv.hex_uint8_t(destination)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EbusSensor),
        cv.GenerateID(CONF_EBUS_ID): cv.use_id(EbusComponent),
        cv.Required(CONF_SENSORS): cv.ensure_list(sensor.sensor_schema().extend(
            {
                cv.GenerateID(): cv.declare_id(EbusSensor),
                cv.Required(CONF_SENSOR_MESSAGE): cv.Schema(
                    {
                        cv.Optional(CONF_SENSOR_MESSAGE_SOURCE): validate_address,
                        cv.Optional(CONF_SENSOR_MESSAGE_DESTINATION): validate_address,
                        cv.Required(CONF_SENSOR_MESSAGE_COMMAND): cv.hex_uint16_t,
                        cv.Required(CONF_SENSOR_MESSAGE_PAYLOAD): cv.Schema([cv.hex_uint8_t]),
                        cv.Optional(CONF_SENSOR_DECODE): cv.Schema(
                            {
                                cv.Optional(CONF_SENSOR_DECODE_POSITION, default=0): cv.int_range(0, 15),
                                cv.Required(CONF_SENSOR_DECODE_BYTES): cv.int_range(1, 4),
                                cv.Required(CONF_SENSOR_DECODE_DIVIDER): cv.float_,
                            }
                        ),
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
        if CONF_SENSOR_MESSAGE_SOURCE in conf[CONF_SENSOR_MESSAGE]:
            cg.add(sens.set_source(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_SOURCE]))

        if CONF_SENSOR_MESSAGE_DESTINATION in conf[CONF_SENSOR_MESSAGE]:
            cg.add(sens.set_destination(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_DESTINATION]))
        cg.add(sens.set_command(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_COMMAND]))
        cg.add(sens.set_payload(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_MESSAGE_PAYLOAD]))
        cg.add(sens.set_response_read_position(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_DECODE][CONF_SENSOR_DECODE_POSITION]))
        cg.add(sens.set_response_read_bytes(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_DECODE][CONF_SENSOR_DECODE_BYTES]))
        cg.add(sens.set_response_read_divider(conf[CONF_SENSOR_MESSAGE][CONF_SENSOR_DECODE][CONF_SENSOR_DECODE_DIVIDER]))


        cg.add(ebus.add_receiver(sens))
        cg.add(ebus.add_sender(sens))
