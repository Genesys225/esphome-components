# esphome-components

ESPHome external components.

## kelon168

Climate (IR) component for AC units that use the 168-bit Kelon protocol —
notably **Tornado** branded split units. The protocol is the 168-bit variant
documented in [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)
(`ir_Kelon.cpp`), distinct from the shorter 48-bit Kelon protocol that already
ships with ESPHome's `climate_ir_lg` / `kelon` components.

Supports:

- Modes: cool, heat, dry, fan-only, auto (smart)
- Fan speeds: auto / low / medium / high
- Vertical swing on/off
- Target temperature 18–30 °C
- Both transmit and receive (state sync from physical remote)

### Usage

Reference this repository from your ESPHome YAML via
[`external_components`](https://esphome.io/components/external_components.html):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Genesys225/esphome-components
      ref: main
    components: [kelon168]

remote_transmitter:
  pin: GPIO4
  carrier_duty_percent: 50%

remote_receiver:
  pin:
    number: GPIO5
    inverted: true
  dump: raw

climate:
  - platform: kelon168
    name: "Living Room AC"
    receiver_id: !secret remote_receiver_id   # optional, omit if no receiver
```

### Notes

- `KELON168_MODEL_BYTE` in `kelon168_climate.h` is set to `0x00`. If your
  physical remote sends a different model byte, adjust it to match — receive
  decoding is tolerant of the value, but transmit will only be byte-identical
  to your remote if it's set correctly.
- Power on/off is encoded as a toggle command in this protocol. The component
  tracks the last-known mode to issue the correct command when the HA state
  changes.

## License

GPLv3 — see [LICENSE](LICENSE). Matches ESPHome's own license so the component
can be upstreamed without relicensing.
