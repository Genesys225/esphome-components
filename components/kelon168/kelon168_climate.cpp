#include "kelon168_climate.h"

namespace esphome {
namespace kelon168 {

uint8_t Kelon168Climate::fan_speed_() const {
  if (!this->fan_mode.has_value()) {
    return (this->model_ == Kelon168Model::TORNADO)
               ? KELON168_TORNADO_FAN_AUTO
               : KELON168_DG11R201_FAN_AUTO;
  }
  if (this->model_ == Kelon168Model::TORNADO) {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_LOW:    return KELON168_TORNADO_FAN_LOW;
      case climate::CLIMATE_FAN_MEDIUM: return KELON168_TORNADO_FAN_MEDIUM;
      case climate::CLIMATE_FAN_HIGH:   return KELON168_TORNADO_FAN_HIGH;
      default:                          return KELON168_TORNADO_FAN_AUTO;
    }
  }
  // DG11R201 (canonical)
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:    return KELON168_DG11R201_FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM: return KELON168_DG11R201_FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:   return KELON168_DG11R201_FAN_HIGH;
    default:                          return KELON168_DG11R201_FAN_AUTO;
  }
}

uint8_t Kelon168Climate::model_byte_() const {
  if (this->model_ == Kelon168Model::TORNADO)
    return KELON168_TORNADO_BYTE18;
  // DG11R201: On bit reflects current mode.
  return (this->mode == climate::CLIMATE_MODE_OFF) ? KELON168_DG11R201_BYTE18_OFF
                                                  : KELON168_DG11R201_BYTE18_ON;
}

bool Kelon168Climate::decode_fan_(uint8_t raw, climate::ClimateFanMode &out) const {
  if (this->model_ == Kelon168Model::TORNADO) {
    switch (raw) {
      case KELON168_TORNADO_FAN_AUTO:   out = climate::CLIMATE_FAN_AUTO;   return true;
      case KELON168_TORNADO_FAN_LOW:    out = climate::CLIMATE_FAN_LOW;    return true;
      case KELON168_TORNADO_FAN_MEDIUM: out = climate::CLIMATE_FAN_MEDIUM; return true;
      case KELON168_TORNADO_FAN_HIGH:   out = climate::CLIMATE_FAN_HIGH;   return true;
      default: return false;
    }
  }
  switch (raw) {
    case KELON168_DG11R201_FAN_AUTO:   out = climate::CLIMATE_FAN_AUTO;   return true;
    case KELON168_DG11R201_FAN_LOW:    out = climate::CLIMATE_FAN_LOW;    return true;
    case KELON168_DG11R201_FAN_MEDIUM: out = climate::CLIMATE_FAN_MEDIUM; return true;
    case KELON168_DG11R201_FAN_HIGH:   out = climate::CLIMATE_FAN_HIGH;   return true;
    default: return false;
  }
}

void Kelon168Climate::build_packet_(uint8_t *pkt, uint8_t cmd) {
  memset(pkt, 0, KELON168_STATE_LEN);

  // Fixed preamble
  pkt[0] = 0x83;
  pkt[1] = 0x06;

  uint8_t speed = fan_speed_();

  // Byte 2: Fan[1:0], Power (toggle, only set on POWER cmd), Sleep, Swing1
  pkt[2] = (speed & 0x03)
         | ((cmd == KELON168_CMD_POWER ? 1u : 0u) << 2)
         | ((this->swing_mode == climate::CLIMATE_SWING_VERTICAL ? 1u : 0u) << 7);

  // Byte 3: Mode[2:0], unused, Temp[3:0]  (temp offset from MIN_TEMP)
  // Power-off: real remote always sends SMART mode + MIN_TEMP regardless of last state
  uint8_t mode_byte, temp_offset;
  if (cmd == KELON168_CMD_POWER && this->mode == climate::CLIMATE_MODE_OFF) {
    mode_byte   = KELON168_MODE_SMART;
    temp_offset = 0;
  } else {
    mode_byte   = ac_mode_();
    temp_offset = static_cast<uint8_t>(this->target_temperature) - KELON168_MIN_TEMP;
  }
  pkt[3] = (mode_byte & 0x07) | ((temp_offset & 0x0F) << 4);

  // Byte 15: command that triggered this transmission
  pkt[15] = cmd;

  // Byte 16: Fan2 (MSB of 3-bit fan speed)
  pkt[16] = ((speed >> 2) & 0x01) << 1;

  // Byte 18: model identifier (+ On bit on DG11R201)
  pkt[18] = model_byte_();

  // Checksums
  pkt[KELON168_SUM1_BYTE] = xor_bytes_(pkt, 2, 11);   // XOR bytes 2–12
  pkt[KELON168_SUM2_BYTE] = xor_bytes_(pkt, 14, 6);   // XOR bytes 14–19
}

void Kelon168Climate::transmit_state() {
  uint8_t cmd = KELON168_CMD_TEMP;

  bool mode_changed = (this->mode != this->last_mode_);
  bool fan_changed  = (this->fan_mode != this->last_fan_);
  bool temp_changed = (this->target_temperature != this->last_temp_);

  if (mode_changed) {
    cmd = (this->mode == climate::CLIMATE_MODE_OFF ||
           this->last_mode_ == climate::CLIMATE_MODE_OFF)
        ? KELON168_CMD_POWER
        : KELON168_CMD_MODE;
  } else if (fan_changed) {
    cmd = KELON168_CMD_FAN;
  } else if (temp_changed) {
    cmd = KELON168_CMD_TEMP;
  }

  uint8_t pkt[KELON168_STATE_LEN];
  build_packet_(pkt, cmd);

  ESP_LOGI("kelon168", "TX cmd=0x%02X pkt: "
           "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
           "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           cmd,
           pkt[0],pkt[1],pkt[2],pkt[3],pkt[4],pkt[5],pkt[6],
           pkt[7],pkt[8],pkt[9],pkt[10],pkt[11],pkt[12],pkt[13],
           pkt[14],pkt[15],pkt[16],pkt[17],pkt[18],pkt[19],pkt[20]);

  auto transmit = this->transmitter_->transmit();
  auto *data    = transmit.get_data();
  data->set_carrier_frequency(38000);
  encode_section_(data, pkt, KELON168_SEC1_SIZE, KELON168_FOOTER_SPACE, true);
  encode_section_(data, pkt + KELON168_SEC1_SIZE, KELON168_SEC2_SIZE, KELON168_FOOTER_SPACE, false);
  encode_section_(data, pkt + KELON168_SEC1_SIZE + KELON168_SEC2_SIZE, KELON168_SEC3_SIZE, KELON168_GAP, false);
  transmit.perform();

  this->last_mode_ = this->mode;
  this->last_fan_  = this->fan_mode;
  this->last_temp_ = this->target_temperature;
}

bool Kelon168Climate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t pkt[KELON168_STATE_LEN] = {};

  // Header
  if (!data.expect_item(KELON168_HDR_MARK, KELON168_HDR_SPACE))
    return false;

  // Decode all three sections sequentially
  const uint8_t section_sizes[3] = {KELON168_SEC1_SIZE, KELON168_SEC2_SIZE, KELON168_SEC3_SIZE};
  uint8_t byte_idx = 0;
  for (uint8_t sec = 0; sec < 3; sec++) {
    for (uint8_t i = 0; i < section_sizes[sec]; i++, byte_idx++) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (data.expect_item(KELON168_BIT_MARK, KELON168_ONE_SPACE))
          pkt[byte_idx] |= (1u << bit);
        else if (!data.expect_item(KELON168_BIT_MARK, KELON168_ZERO_SPACE))
          return false;
      }
    }
    if (sec < 2 && !data.expect_item(KELON168_BIT_MARK, KELON168_FOOTER_SPACE))
      return false;
  }

  // Validate preamble and checksums
  if (pkt[0] != 0x83 || pkt[1] != 0x06)
    return false;
  if (xor_bytes_(pkt, 2, 11) != pkt[KELON168_SUM1_BYTE])
    return false;
  if (xor_bytes_(pkt, 14, 6) != pkt[KELON168_SUM2_BYTE])
    return false;

  uint8_t cmd      = pkt[15];
  uint8_t fan_raw  = (pkt[2] & 0x03) | ((pkt[16] >> 1 & 1) << 2);
  bool    swing    = (pkt[2] >> 7) & 1;
  uint8_t mode_raw = pkt[3] & 0x07;
  uint8_t temp     = (pkt[3] >> 4) + KELON168_MIN_TEMP;

  // Power-off: both real remote and phone send SMART+MIN_TEMP when turning off
  if (cmd == KELON168_CMD_POWER &&
      mode_raw == KELON168_MODE_SMART &&
      temp == KELON168_MIN_TEMP) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->last_mode_ = this->mode;
    this->publish_state();
    return true;
  }

  // Mode
  switch (mode_raw) {
    case KELON168_MODE_HEAT:  this->mode = climate::CLIMATE_MODE_HEAT; break;
    case KELON168_MODE_SMART: this->mode = climate::CLIMATE_MODE_HEAT_COOL; break;
    case KELON168_MODE_COOL:  this->mode = climate::CLIMATE_MODE_COOL; break;
    case KELON168_MODE_DRY:   this->mode = climate::CLIMATE_MODE_DRY; break;
    case KELON168_MODE_FAN:   this->mode = climate::CLIMATE_MODE_FAN_ONLY; break;
    default: return false;
  }

  // Fan (model-dependent decode)
  climate::ClimateFanMode decoded_fan;
  if (!decode_fan_(fan_raw, decoded_fan))
    return false;
  this->fan_mode = decoded_fan;

  // Temperature
  if (temp >= KELON168_MIN_TEMP && temp <= KELON168_MAX_TEMP)
    this->target_temperature = temp;

  // Swing
  this->swing_mode = swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;

  // Sync so transmit_state doesn't re-send
  this->last_mode_ = this->mode;
  this->last_fan_  = this->fan_mode;
  this->last_temp_ = this->target_temperature;

  this->publish_state();
  return true;
}

}  // namespace kelon168
}  // namespace esphome
