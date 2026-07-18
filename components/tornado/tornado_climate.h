#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace tornado {

// ── Timing constants ────────────────────────────────────────────────────────
// Derived from the 168-bit Kelon protocol (IRremoteESP8266 src/ir_Kelon.cpp) as
// an implementation base, then matched against signals captured from a real
// Tornado remote. Distinct from the shorter 48-bit Kelon protocol in ESPHome.
static const uint16_t TORNADO_HDR_MARK     = 9000;    // µs
static const uint16_t TORNADO_HDR_SPACE    = 4600;    // µs
static const uint16_t TORNADO_BIT_MARK     = 560;     // µs
static const uint16_t TORNADO_ONE_SPACE    = 1680;    // µs
static const uint16_t TORNADO_ZERO_SPACE   = 600;     // µs
static const uint32_t TORNADO_FOOTER_SPACE = 8000;    // µs – inter-section gap
static const uint32_t TORNADO_GAP          = 200000;  // µs – end of message

// ── Protocol structure ──────────────────────────────────────────────────────
static const uint8_t TORNADO_STATE_LEN  = 21;
static const uint8_t TORNADO_SEC1_SIZE  = 6;
static const uint8_t TORNADO_SEC2_SIZE  = 8;
static const uint8_t TORNADO_SEC3_SIZE  = 7;
static const uint8_t TORNADO_SUM1_BYTE  = 13;   // XOR of bytes 2–12
static const uint8_t TORNADO_SUM2_BYTE  = 20;   // XOR of bytes 14–19

// ── Mode constants ──────────────────────────────────────────────────────────
static const uint8_t TORNADO_MODE_HEAT  = 0;
static const uint8_t TORNADO_MODE_SMART = 1;
static const uint8_t TORNADO_MODE_COOL  = 2;
static const uint8_t TORNADO_MODE_DRY   = 3;
static const uint8_t TORNADO_MODE_FAN   = 4;

// ── Fan speed codes (3-bit, split across byte 2 [low 2] + byte 16 [high 1]) ──
static const uint8_t TORNADO_FAN_AUTO   = 0;
static const uint8_t TORNADO_FAN_HIGH   = 1;
static const uint8_t TORNADO_FAN_MEDIUM = 2;
static const uint8_t TORNADO_FAN_LOW    = 3;

// ── Byte 18 ─────────────────────────────────────────────────────────────────
// Captured as 0x00 on the Tornado unit (Model1/On/Model2 fields ignored).
static const uint8_t TORNADO_BYTE18 = 0x00;

// ── Command byte (byte 15) ──────────────────────────────────────────────────
static const uint8_t TORNADO_CMD_POWER  = 0x01;
static const uint8_t TORNADO_CMD_TEMP   = 0x02;
static const uint8_t TORNADO_CMD_MODE   = 0x06;
static const uint8_t TORNADO_CMD_FAN    = 0x11;

// ── Temperature range (protocol max: 16–32 °C; clamped to ESPHome-conservative 18–30) ──
static const uint8_t TORNADO_MIN_TEMP = 18;
static const uint8_t TORNADO_MAX_TEMP = 30;

// ─────────────────────────────────────────────────────────────────────────

class TornadoClimate : public climate_ir::ClimateIR {
 public:
  TornadoClimate()
      : climate_ir::ClimateIR(
            TORNADO_MIN_TEMP,
            TORNADO_MAX_TEMP,
            1.0f,   // temperature step
            true,   // supports dry
            true,   // supports fan-only
            // Fan modes
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
            // Swing modes
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  void setup() override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

 private:
  static uint8_t xor_bytes_(const uint8_t *data, uint8_t start, uint8_t len) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < len; i++)
      result ^= data[start + i];
    return result;
  }

  // Map current ClimateFanMode -> raw 3-bit fan code.
  uint8_t fan_speed_() const;

  // Decode raw 3-bit fan code -> ClimateFanMode. Returns false on an unknown code.
  bool decode_fan_(uint8_t raw, climate::ClimateFanMode &out) const;

  uint8_t ac_mode_() const {
    switch (this->mode) {
      case climate::CLIMATE_MODE_HEAT:      return TORNADO_MODE_HEAT;
      case climate::CLIMATE_MODE_DRY:       return TORNADO_MODE_DRY;
      case climate::CLIMATE_MODE_FAN_ONLY:  return TORNADO_MODE_FAN;
      case climate::CLIMATE_MODE_HEAT_COOL: return TORNADO_MODE_SMART;
      default:                              return TORNADO_MODE_COOL;
    }
  }

  void build_packet_(uint8_t *pkt, uint8_t cmd);

  static void encode_bit_(remote_base::RemoteTransmitData *data, bool one) {
    data->mark(TORNADO_BIT_MARK);
    data->space(one ? TORNADO_ONE_SPACE : TORNADO_ZERO_SPACE);
  }

  static void encode_section_(remote_base::RemoteTransmitData *data,
                               const uint8_t *bytes, uint8_t len,
                               uint32_t footer_space, bool with_header) {
    if (with_header) {
      data->mark(TORNADO_HDR_MARK);
      data->space(TORNADO_HDR_SPACE);
    }
    for (uint8_t i = 0; i < len; i++)
      for (uint8_t bit = 0; bit < 8; bit++)
        encode_bit_(data, (bytes[i] >> bit) & 1);

    data->mark(TORNADO_BIT_MARK);
    data->space(footer_space);
  }

  climate::ClimateMode      last_mode_{climate::CLIMATE_MODE_OFF};
  optional<climate::ClimateFanMode> last_fan_{};
  float                     last_temp_{24.0f};
};

}  // namespace tornado
}  // namespace esphome
