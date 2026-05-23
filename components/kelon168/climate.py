import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_MODEL

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@Genesys225"]

kelon168_ns = cg.esphome_ns.namespace("kelon168")
Kelon168Climate = kelon168_ns.class_("Kelon168Climate", climate_ir.ClimateIR)
Kelon168Model = kelon168_ns.enum("Kelon168Model", is_class=True)

MODELS = {
    # Reverse-engineered from a Tornado-branded unit. Default because it is
    # the only variant we have actually validated on hardware.
    "tornado": Kelon168Model.TORNADO,
    # Canonical 168-bit Kelon encoding per IRremoteESP8266 (Kelon DG11R2-01,
    # reportedly also RCH-R0Y3 and Hisense AST-09UW4RVETG00A). Not field-
    # tested in this component — provided for completeness.
    "dg11r201": Kelon168Model.DG11R201,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(Kelon168Climate).extend(
    {
        cv.Optional(CONF_MODEL, default="tornado"): cv.enum(MODELS, lower=True),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))
