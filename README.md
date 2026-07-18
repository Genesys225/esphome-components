# esphome-components

ESPHome external components.

## tornado

Climate (IR) component for **Tornado**-branded split AC units. The wire protocol
(timing, framing, checksums) was built on the 168-bit Kelon protocol documented in
[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) (`ir_Kelon.cpp`)
as an implementation base — the existing remote whose header timings best matched
the signals captured from the real Tornado remote — then adapted and validated
against those captured signals. It is distinct from the shorter 48-bit Kelon
protocol that already ships with ESPHome.

The encoding (fan-speed code mapping, byte-18 value) was matched to a real Tornado
remote and validated on real hardware.

Supports:

- Modes: cool, heat, dry, fan-only, auto (smart)
- Fan speeds: auto / low / medium / high
- Vertical swing on/off
- Target temperature 18–30 °C
- Both transmit and receive (state sync from physical remote)

Not yet implemented: sleep, super/turbo, light, on-/off-timers, iFeel, Swing2,
fan min/max speeds.

### Usage

Reference this repository from your ESPHome YAML via
[`external_components`](https://esphome.io/components/external_components.html):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Genesys225/esphome-components
      ref: main
    components: [tornado]

remote_transmitter:
  pin: GPIO4
  carrier_duty_percent: 50%

remote_receiver:
  pin:
    number: GPIO5
    inverted: true
  dump: raw

climate:
  - platform: tornado
    name: "Living Room AC"
    receiver_id: !secret remote_receiver_id   # optional, omit if no receiver
```

### Notes

- Power on/off is encoded as a toggle command in this protocol. The component
  tracks the last-known mode to issue the correct command when the HA state
  changes.
- The protocol allows temperatures 16–32 °C; this component currently clamps to
  18–30 °C (ESPHome-conservative). Easy to widen if needed.

## License

GPLv3 — see [LICENSE](LICENSE). Matches ESPHome's own license so the component
can be upstreamed without relicensing.
