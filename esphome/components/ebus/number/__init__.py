import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number

from esphome.const import (
    CONF_BYTES,
)

from .. import (
    CONF_ID,
    CONF_EBUS_ID,
    CONF_TELEGRAM,
    CONF_DECODE,
    ebus_ns,
    create_telegram_schema,
    item_config,
)

AUTO_LOAD = ["ebus"]

EbusNumber = ebus_ns.class_("EbusNumber", number.Number, cg.Component)

CONF_DIVIDER = "divider"

CONFIG_SCHEMA = (
    number.number_schema(EbusNumber).extend(
        create_telegram_schema(
            {
                cv.Required(CONF_BYTES): cv.int_range(1, 4),
                cv.Required(CONF_DIVIDER): cv.float_,
            }
        )
    )
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await number.register_number(
        var,
        config,
        min_value=0,  #TODO: config[CONF_MIN_VALUE],
        max_value=100,  #TODO: config[CONF_MAX_VALUE],
        step=0.1,  #TODO: config[CONF_STEP],
    )


    ebus = await cg.get_variable(config[CONF_EBUS_ID])
    # var = await number.register_number(config)
    # item_config(sens, config)

    # cg.add(sens.set_response_read_bytes(config[CONF_TELEGRAM][CONF_DECODE][CONF_BYTES]))
    # cg.add(
    #     sens.set_response_read_divider(config[CONF_TELEGRAM][CONF_DECODE][CONF_DIVIDER])
    # )
    cg.add(ebus.add_sender(var))
