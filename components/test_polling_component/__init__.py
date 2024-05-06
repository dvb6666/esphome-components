import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import const, automation
from esphome.const import (
    CONF_ID,
    CONF_ON_UPDATE,
    CONF_TRIGGER_ID,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@dvb666"]
MULTI_CONF = True

test_polling_component_ns = cg.esphome_ns.namespace("test_polling_component")
TestPollingComponent = test_polling_component_ns.class_(
    "TestPollingComponent", cg.Component
)
TestPollingComponentConstRef = TestPollingComponent.operator("ref").operator("const")

TestPollingComponentUpdateTrigger = test_polling_component_ns.class_(
    "TestPollingComponentUpdateTrigger", automation.Trigger.template(cg.uint32),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TestPollingComponent),
            cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TestPollingComponentUpdateTrigger),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("10min"))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for conf in config.get(CONF_ON_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "x"), (TestPollingComponentConstRef, "xthis")], conf)
