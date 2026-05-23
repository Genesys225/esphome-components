#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace kelon168 {

// ── Timing constants (from IRremoteESP8266/src/ir_Kelon.cpp) ──────────────
static const uint16_t KELON168_HDR_MARK     = 9000;   // µs
static const uint16_t KELON168_HDR_SPACE    = 4600;   // µs
static const uint16_t KELON168_BIT_MARK     = 560;    // µs
static const uint16_t KELON168_ONE_SPACE    = 1680;   // µs
static const uint16_t KELON168_ZERO_SPACE   = 600;    // µs
static const uint32_t KELON168_FOOTER_SPACE = 8000;   // µs – inter-section gap
static const uint32_t KELON168_GAP         = 200000;  // µs – end of message

// ── Protocol structure ────────────────────────────────────────────────────
static const uint8_t KELON168_STATE_LEN  = 21;
static const uint8_t KELON168_SEC1_SIZE  = 6;
static const uint8_t KELON168_SEC2_SIZE  = 8;
static const uint8_t KELON168_SEC3_SIZE  = 7;
static const uint8_t KELON168_SUM1_BYTE  = 13;   // XOR of bytes 2–12
static const uint8_t KELON168_SUM2_BYTE  = 20;   // XOR of bytes 14–19

// ── Mode constants ────────────────────────────────────────────────────────
static const uint8_t KELON168_MODE_HEAT  = 0;
static const uint8_t KELON168_MODE_SMART = 1;
static const uint8_t KELON168_MODE_COOL  = 2;
static const uint8_t KELON168_MODE_DRY   = 3;
static const uint8_t KELON168_MODE_FAN   = 4;

// ── Fan speed codes (3-bit, split across byte 2 [low 2] + byte 16 [high 1]) ──
//
// TORNADO variant (reverse-engineered):
static const uint8_t KELON168_TORNADO_FAN_AUTO   = 0;
static const uint8_t KELON168_TORNADO_FAN_HIGH   = 1;
static const uint8_t KELON168_TORNADO_FAN_MEDIUM = 2;
static const uint8_t KELON168_TORNADO_FAN_LOW    = 3;
//
// DG11R201 variant (canonical IRremoteESP8266 reference):
static const uint8_t KELON168_DG11R201_FAN_AUTO   = 0;
static const uint8_t KELON168_DG11R201_FAN_MIN    = 1;
static const uint8_t KELON168_DG11R201_FAN_LOW    = 2;
static const uint8_t KELON168_DG11R201_FAN_MEDIUM = 3;
static const uint8_t KELON168_DG11R201_FAN_HIGH   = 4;
static const uint8_t KELON168_DG11R201_FAN_MAX    = 5;

// ── Byte 18 (model identifier + "On" bit) ─────────────────────────────────
//   TORNADO:  captured as 0x00 (the unit ignores Model1/On/Model2)
//   DG11R201: Model1=8, Model2=1, On bit set when AC is on
//             → 0x38 (on) / 0x28 (off)
static const uint8_t KELON168_TORNADO_BYTE18      = 0x00;
static const uint8_t KELON168_DG11R201_BYTE18_OFF = 0x28;  // Model1=8, Model2=1, On=0
static const uint8_t KELON168_DG11R201_BYTE18_ON  = 0x38;  // Model1=8, Model2=1, On=1

// ── Command byte (byte 15) ────────────────────────────────────────────────
static const uint8_t KELON168_CMD_POWER  = 0x01;
static const uint8_t KELON168_CMD_TEMP   = 0x02;
static const uint8_t KELON168_CMD_MODE   = 0x06;
static const uint8_t KELON168_CMD_FAN    = 0x11;

// ── Temperature range (protocol max: 16–32 °C; clamped to ESPHome-conservative 18–30) ──
static const uint8_t KELON168_MIN_TEMP = 18;
static const uint8_t KELON168_MAX_TEMP = 30;

// ── Remote/AC model variants ──────────────────────────────────────────────
//   TORNADO  — reverse-engineered from a Tornado-branded 168-bit Kelon unit
//              (the only variant we've actually validated). Fan codes and
//              byte 18 differ from the upstream reference.
//   DG11R201 — canonical encoding per IRremoteESP8266's ir_Kelon.cpp. Also
//              reportedly used by Kelon RCH-R0Y3 and Hisense AST-09UW4RVETG00A.
//              Included for completeness; not field-tested by us.
enum class Kelon168Model : uint8_t {
  TORNADO  = 0,
  DG11R201 = 1,
};

// ─────────────────────────────────────────────────────────────────────────

class Kelon168Climate : public climate_ir::ClimateIR {
 public:
  Kelon168Climate()
      : climate_ir::ClimateIR(
            KELON168_MIN_TEMP,
            KELON168_MAX_TEMP,
            1.0f,   // temperature step
            true,   // supports dry
            true,   // supports fan-only
            // Fan modes
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
            // Swing modes
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void set_model(Kelon168Model model) { this->model_ = model; }

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

 private:
  static uint8_t xor_bytes_(const uint8_t *data, uint8_t start, uint8_t len) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < len; i++)
      result ^= data[start + i];
    return result;
  }

  // Map current ClimateFanMode -> raw 3-bit fan code per active model.
  uint8_t fan_speed_() const;

  // Byte 18 (model identifier + On bit) per active model.
  uint8_t model_byte_() const;

  // Decode raw 3-bit fan code -> ClimateFanMode per active model.
  // Returns false on an unknown code for the current model.
  bool decode_fan_(uint8_t raw, climate::ClimateFanMode &out) const;

  uint8_t ac_mode_() const {
    switch (this->mode) {
      case climate::CLIMATE_MODE_HEAT:      return KELON168_MODE_HEAT;
      case climate::CLIMATE_MODE_DRY:       return KELON168_MODE_DRY;
      case climate::CLIMATE_MODE_FAN_ONLY:  return KELON168_MODE_FAN;
      case climate::CLIMATE_MODE_HEAT_COOL: return KELON168_MODE_SMART;
      default:                              return KELON168_MODE_COOL;
    }
  }

  void build_packet_(uint8_t *pkt, uint8_t cmd);

  static void encode_bit_(remote_base::RemoteTransmitData *data, bool one) {
    data->mark(KELON168_BIT_MARK);
    data->space(one ? KELON168_ONE_SPACE : KELON168_ZERO_SPACE);
  }

  static void encode_section_(remote_base::RemoteTransmitData *data,
                               const uint8_t *bytes, uint8_t len,
                               uint32_t footer_space, bool with_header) {
    if (with_header) {
      data->mark(KELON168_HDR_MARK);
      data->space(KELON168_HDR_SPACE);
    }
    for (uint8_t i = 0; i < len; i++)
      for (uint8_t bit = 0; bit < 8; bit++)
        encode_bit_(data, (bytes[i] >> bit) & 1);

    data->mark(KELON168_BIT_MARK);
    data->space(footer_space);
  }

  climate::ClimateMode      last_mode_{climate::CLIMATE_MODE_OFF};
  optional<climate::ClimateFanMode> last_fan_{};
  float                     last_temp_{24.0f};
  Kelon168Model             model_{Kelon168Model::TORNADO};
};

}  // namespace kelon168
}  // namespace esphome
