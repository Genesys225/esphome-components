# esphome-components

ESPHome external components.

## kelon168

Climate (IR) component for AC units that use the 168-bit Kelon protocol family —
notably **Tornado**-branded split units, with optional support for the canonical
Kelon DG11R2-01 encoding. The wire protocol (timing, framing, checksums) is the
168-bit variant documented in [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)
(`ir_Kelon.cpp`), distinct from the shorter 48-bit Kelon protocol that already
ships with ESPHome.

The Tornado encoding was reverse-engineered from a real remote and diverges from
the upstream reference in fan-speed code mapping and the byte-18 ("model" / "On")
value. Both variants are selectable via the `model:` config option below.

Supports:

- Modes: cool, heat, dry, fan-only, auto (smart)
- Fan speeds: auto / low / medium / high
- Vertical swing on/off
- Target temperature 18–30 °C
- Both transmit and receive (state sync from physical remote)

Not yet implemented (Kelon protocol features present in upstream but not ported):
sleep, super/turbo, light, on-/off-timers, iFeel, Swing2, fan min/max speeds.

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
    model: tornado                            # default; or "dg11r201"
    receiver_id: !secret remote_receiver_id   # optional, omit if no receiver
```

### `model:` option

| Value      | Description |
|------------|-------------|
| `tornado`  | **(default)** Reverse-engineered from a Tornado-branded 168-bit Kelon unit. The only variant validated on real hardware. Fan codes are remapped (`Low=3, Med=2, High=1`) and byte 18 is always `0x00`. |
| `dg11r201` | Canonical 168-bit Kelon encoding per IRremoteESP8266's `ir_Kelon.cpp`. Also reportedly used by Kelon RCH-R0Y3 and Hisense AST-09UW4RVETG00A. Fan codes follow the protocol spec (`Low=2, Med=3, High=4`) and byte 18 includes the `On` bit (`0x38` when on, `0x28` when off). **Not field-tested by this component's author** — if you have one of these remotes and try it, please open an issue with results. |

### Notes

- Power on/off is encoded as a toggle command in this protocol. The component
  tracks the last-known mode to issue the correct command when the HA state
  changes.
- The protocol allows temperatures 16–32 °C; this component currently clamps to
  18–30 °C (ESPHome-conservative). Easy to widen if needed.

## License

GPLv3 — see [LICENSE](LICENSE). Matches ESPHome's own license so the component
can be upstreamed without relicensing.
