#include "tornado_climate.h"

namespace esphome {
namespace tornado {

uint8_t TornadoClimate::fan_speed_() const {
  if (!this->fan_mode.has_value())
    return TORNADO_FAN_AUTO;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:    return TORNADO_FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM: return TORNADO_FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:   return TORNADO_FAN_HIGH;
    default:                          return TORNADO_FAN_AUTO;
  }
}

bool TornadoClimate::decode_fan_(uint8_t raw, climate::ClimateFanMode &out) const {
  switch (raw) {
    case TORNADO_FAN_AUTO:   out = climate::CLIMATE_FAN_AUTO;   return true;
    case TORNADO_FAN_LOW:    out = climate::CLIMATE_FAN_LOW;    return true;
    case TORNADO_FAN_MEDIUM: out = climate::CLIMATE_FAN_MEDIUM; return true;
    case TORNADO_FAN_HIGH:   out = climate::CLIMATE_FAN_HIGH;   return true;
    default: return false;
  }
}

void TornadoClimate::setup() {
  // Let the base class restore persisted state (mode / fan / temperature /
  // swing) or apply its defaults.
  climate_ir::ClimateIR::setup();

  // Seed the "last transmitted" shadow state from the restored live state.
  // transmit_state() picks the IR command byte by diffing the live state
  // against these fields; without this seed they keep their member-initializer
  // defaults (OFF / nullopt / 24 °C) and the first post-boot control() call can
  // diff against a phantom "previous" state and emit a spurious command (e.g. a
  // POWER toggle that turns an already-running unit off).
  this->last_mode_ = this->mode;
  this->last_fan_ = this->fan_mode;
  this->last_temp_ = this->target_temperature;
}

void TornadoClimate::build_packet_(uint8_t *pkt, uint8_t cmd) {
  memset(pkt, 0, TORNADO_STATE_LEN);

  // Fixed preamble
  pkt[0] = 0x83;
  pkt[1] = 0x06;

  uint8_t speed = fan_speed_();

  // Byte 2: Fan[1:0], Power (toggle, only set on POWER cmd), Sleep, Swing1
  pkt[2] = (speed & 0x03)
         | ((cmd == TORNADO_CMD_POWER ? 1u : 0u) << 2)
         | ((this->swing_mode == climate::CLIMATE_SWING_VERTICAL ? 1u : 0u) << 7);

  // Byte 3: Mode[2:0], unused, Temp[3:0]  (temp offset from MIN_TEMP)
  // Power-off: real remote always sends SMART mode + MIN_TEMP regardless of last state
  uint8_t mode_byte, temp_offset;
  if (cmd == TORNADO_CMD_POWER && this->mode == climate::CLIMATE_MODE_OFF) {
    mode_byte   = TORNADO_MODE_SMART;
    temp_offset = 0;
  } else {
    mode_byte   = ac_mode_();
    temp_offset = static_cast<uint8_t>(this->target_temperature) - TORNADO_MIN_TEMP;
  }
  pkt[3] = (mode_byte & 0x07) | ((temp_offset & 0x0F) << 4);

  // Byte 15: command that triggered this transmission
  pkt[15] = cmd;

  // Byte 16: Fan2 (MSB of 3-bit fan speed)
  pkt[16] = ((speed >> 2) & 0x01) << 1;

  // Byte 18: model identifier (captured as 0x00 on the Tornado unit)
  pkt[18] = TORNADO_BYTE18;

  // Checksums
  pkt[TORNADO_SUM1_BYTE] = xor_bytes_(pkt, 2, 11);   // XOR bytes 2–12
  pkt[TORNADO_SUM2_BYTE] = xor_bytes_(pkt, 14, 6);   // XOR bytes 14–19
}

void TornadoClimate::transmit_state() {
  uint8_t cmd = TORNADO_CMD_TEMP;

  bool mode_changed = (this->mode != this->last_mode_);
  bool fan_changed  = (this->fan_mode != this->last_fan_);
  bool temp_changed = (this->target_temperature != this->last_temp_);

  if (mode_changed) {
    cmd = (this->mode == climate::CLIMATE_MODE_OFF ||
           this->last_mode_ == climate::CLIMATE_MODE_OFF)
        ? TORNADO_CMD_POWER
        : TORNADO_CMD_MODE;
  } else if (fan_changed) {
    cmd = TORNADO_CMD_FAN;
  } else if (temp_changed) {
    cmd = TORNADO_CMD_TEMP;
  }

  // If the unit is off and this is not a power transition, there is no valid
  // frame to represent a fan/temp/swing change — "off" is only expressible via
  // the POWER toggle, so any active-mode frame emitted here would be
  // semantically wrong (and misdecodable as an on-state). Emit nothing: the new
  // set-point already lives in this->* (published by control()) and is carried
  // by the next power-on frame. Advance the shadow state so later diffs stay
  // consistent.
  if (this->mode == climate::CLIMATE_MODE_OFF && cmd != TORNADO_CMD_POWER) {
    this->last_mode_ = this->mode;
    this->last_fan_ = this->fan_mode;
    this->last_temp_ = this->target_temperature;
    return;
  }

  uint8_t pkt[TORNADO_STATE_LEN];
  build_packet_(pkt, cmd);

  ESP_LOGI("tornado", "TX cmd=0x%02X pkt: "
           "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
           "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           cmd,
           pkt[0],pkt[1],pkt[2],pkt[3],pkt[4],pkt[5],pkt[6],
           pkt[7],pkt[8],pkt[9],pkt[10],pkt[11],pkt[12],pkt[13],
           pkt[14],pkt[15],pkt[16],pkt[17],pkt[18],pkt[19],pkt[20]);

  auto transmit = this->transmitter_->transmit();
  auto *data    = transmit.get_data();
  data->set_carrier_frequency(38000);
  encode_section_(data, pkt, TORNADO_SEC1_SIZE, TORNADO_FOOTER_SPACE, true);
  encode_section_(data, pkt + TORNADO_SEC1_SIZE, TORNADO_SEC2_SIZE, TORNADO_FOOTER_SPACE, false);
  encode_section_(data, pkt + TORNADO_SEC1_SIZE + TORNADO_SEC2_SIZE, TORNADO_SEC3_SIZE, TORNADO_GAP, false);
  transmit.perform();

  this->last_mode_ = this->mode;
  this->last_fan_  = this->fan_mode;
  this->last_temp_ = this->target_temperature;
}

bool TornadoClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t pkt[TORNADO_STATE_LEN] = {};

  // Header
  if (!data.expect_item(TORNADO_HDR_MARK, TORNADO_HDR_SPACE))
    return false;

  // Decode all three sections sequentially
  const uint8_t section_sizes[3] = {TORNADO_SEC1_SIZE, TORNADO_SEC2_SIZE, TORNADO_SEC3_SIZE};
  uint8_t byte_idx = 0;
  for (uint8_t sec = 0; sec < 3; sec++) {
    for (uint8_t i = 0; i < section_sizes[sec]; i++, byte_idx++) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (data.expect_item(TORNADO_BIT_MARK, TORNADO_ONE_SPACE))
          pkt[byte_idx] |= (1u << bit);
        else if (!data.expect_item(TORNADO_BIT_MARK, TORNADO_ZERO_SPACE))
          return false;
      }
    }
    if (sec < 2 && !data.expect_item(TORNADO_BIT_MARK, TORNADO_FOOTER_SPACE))
      return false;
  }

  // Validate preamble and checksums
  if (pkt[0] != 0x83 || pkt[1] != 0x06)
    return false;
  if (xor_bytes_(pkt, 2, 11) != pkt[TORNADO_SUM1_BYTE])
    return false;
  if (xor_bytes_(pkt, 14, 6) != pkt[TORNADO_SUM2_BYTE])
    return false;

  uint8_t cmd      = pkt[15];
  uint8_t fan_raw  = (pkt[2] & 0x03) | ((pkt[16] >> 1 & 1) << 2);
  bool    swing    = (pkt[2] >> 7) & 1;
  uint8_t mode_raw = pkt[3] & 0x07;
  uint8_t temp     = (pkt[3] >> 4) + TORNADO_MIN_TEMP;

  // Power-off: both real remote and phone send SMART+MIN_TEMP when turning off
  if (cmd == TORNADO_CMD_POWER &&
      mode_raw == TORNADO_MODE_SMART &&
      temp == TORNADO_MIN_TEMP) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->last_mode_ = this->mode;
    this->publish_state();
    return true;
  }

  // Decode mode and fan into locals BEFORE mutating any live state. A frame can
  // pass the checksum yet carry a fan code outside the defined set. Rejecting it
  // atomically here leaves this->mode and the shadow state untouched, instead of
  // half-applying the mode and then bailing — which would desync last_mode_ and
  // make the next transmit_state() emit the wrong command.
  climate::ClimateMode decoded_mode = climate::CLIMATE_MODE_OFF;
  switch (mode_raw) {
    case TORNADO_MODE_HEAT:  decoded_mode = climate::CLIMATE_MODE_HEAT;      break;
    case TORNADO_MODE_SMART: decoded_mode = climate::CLIMATE_MODE_HEAT_COOL; break;
    case TORNADO_MODE_COOL:  decoded_mode = climate::CLIMATE_MODE_COOL;      break;
    case TORNADO_MODE_DRY:   decoded_mode = climate::CLIMATE_MODE_DRY;       break;
    case TORNADO_MODE_FAN:   decoded_mode = climate::CLIMATE_MODE_FAN_ONLY;  break;
    default: return false;
  }

  climate::ClimateFanMode decoded_fan;
  if (!decode_fan_(fan_raw, decoded_fan))
    return false;

  // All fields decoded successfully — commit atomically.
  this->mode = decoded_mode;
  this->fan_mode = decoded_fan;
  if (temp >= TORNADO_MIN_TEMP && temp <= TORNADO_MAX_TEMP)
    this->target_temperature = temp;
  this->swing_mode = swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;

  // Sync so transmit_state doesn't re-send.
  this->last_mode_ = this->mode;
  this->last_fan_  = this->fan_mode;
  this->last_temp_ = this->target_temperature;

  this->publish_state();
  return true;
}

}  // namespace tornado
}  // namespace esphome
