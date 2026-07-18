import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@Genesys225"]

tornado_ns = cg.esphome_ns.namespace("tornado")
TornadoClimate = tornado_ns.class_("TornadoClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(TornadoClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)
