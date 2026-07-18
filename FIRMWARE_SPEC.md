# ORBit Firmware Specification — rev A (2026-07-17)

Handoff spec for firmware development on the ORBit capacitive-touch trackball bar.
Hardware and geometry are FROZEN at this revision. The wiring section of the project
README (v7 cap-touch) is authoritative for harness details; this file is authoritative
for firmware scope and contracts.

## 1. Device summary

Low-profile wireless trackball bar, 135.2 × 34.7 × ~19 mm (138 × 37.5 with TPU sleeve),
XIAO nRF52840 Sense Plus. No moving switches: optical trackball, magnetic scroll ring
(hall quadrature), 6 capacitive touch zones, 2-pad capacitive slider. Outputs: 8-LED
amber glow chain, LRA haptics, I2S speaker. Identity: quiet dark desk object — amber
light only, subtle haptics, sound opt-in. Every feature must work with the sleeve fitted.

## 2. Platform + pin map (authoritative)

| Pin | Net | Device |
|---|---|---|
| D0 | TOUCH_INT | CAP1188 ALERT |
| D1 | TOUCH_RST | CAP1188 RESET (or tie high) |
| D2 / D3 | SCROLL_A/B | hall latches SS460S (quadrature, 10k pull-ups to 3V3 on harness) |
| D4 / D5 | SDA / SCL | shared I2C: CAP1188 @0x29, DRV2605L @0x5A |
| D6 | LED_DIN | SK6812MINI-E chain ×8 (330–470 Ω series; battery-voltage strip, 3.3 V logic OK) |
| D7 | MOT | PMW3610 motion IRQ |
| D8 / D9 / D10 | SCK / nCS / SDIO | PMW3610 3-wire SPI (park MISO on unused pin; reads on SDIO) |
| EXT1 / EXT2 / EXT3 | I2S BCLK / LRC / DIN | MAX98357A |
| EXT4 | AMP_SD | MAX98357A shutdown (drive low when idle) |
| B+ / B− | LiPo 602040 450 mAh protected | hard slide switch inline on B+ |

Sensor board: randware/siderakb PMW3610 breakout (23.5 x 31.5 x 1.6, optical center =
board center, M3 holes on a 17.5 x 25.5 grid). Mounted component-side down on the v2
sled; lens reference plane at device z8.6, Z = 2.4 mm to the ball (datasheet 2.2-2.6).

Onboard (XIAO Sense): LSM6DS3TR-C 6-axis IMU (internal I2C, wake-on-motion), PDM mic,
2 MB QSPI flash (P25Q16H), USB-C, LiPo charger (4.2 V). No DAC (all audio via I2S amp).
No magnetometer (tilt-relative gestures only; absolute heading impossible).

## 3. Physical → firmware map

Coordinates: device mm; x −67.6 (west/USB) … +67.6 (east); y −17.35 (front, glow-dot
edge) … +17.35 (rear, slider edge).

| Surface | Physical | Firmware contract |
|---|---|---|
| Trackball | Ø25 ball at z11, PMW3610 lens ref plane z8.6 (Z=2.4 to ball, datasheet 2.2–2.6) | REL X/Y, CPI 600 default; set swap/invert flags at bring-up (board mounts component-side down, long axis = device x) |
| Crown | Ø33.8 ring, 12 alternating magnets, halls at (6.2,14.35)/(11.75,14.35) | quadrature 24 edges/rev; continuous scroll |
| Touch zones | foil electrodes under shell caps; finger stack = 1.0 mm TPU + 1.2 mm shell | CS1 L-click (x38.6–50) · CS2 R-click (x51.5–65 rear) · CS3 M-click (x51.5–65 front) · CS4 back (−48…−28) · CS5 fwd (−26…−8) · CS6 PTT (25.7–37). Act on press <10 ms with haptic |
| Slider | CS7+CS8 elongated pads, rear edge x41–59 | v1: half-strip taps (vol/scroll, both=mute); v2: interpolated absolute position |
| Glow | 8× SK6812 in shell pockets w/ funnels → sleeve lenses | index 0–5 = zone dots WEST→EAST (positionally over CS4,CS5,CS6,CS1,CS3,CS2), 6–7 = slider (x45.5/54.5). Amber #FFB457 family ONLY; ≤30% on battery; off in sleep |
| Haptics | LRA Ø10×4 at (−52,0), DRV2605L | per-zone effect identity: sharp click L/R, soft tick back/fwd, double-tick PTT, micro-tick per scroll/profile step. Run LRA auto-cal once, persist |
| Speaker | 15×11 8Ω 0.7W in deck (50.5,−6.5), floor vent | I2S 16-bit mono clips from QSPI; chimes ≤1 s, prompts ≤3 s; AMP_SD low when idle |
| USB | port high on west wall through sleeve window | charge + HID; later MSC/CDC/UAC composite |
| IMU | west end, board component-side down | wake-on-motion = sleep exit; lift-to-lock; double-tap; tilt-hold gestures |

Hardware gotchas that masquerade as firmware bugs:
- Speaker magnet is 15.3 mm from nearest hall latch → phantom scroll during audio = magnetic coupling (hardware fix), not code.
- Cap-sense MUST be tuned with the TPU sleeve fitted (dielectric stack roughly halves raw counts vs bare bench).

## 4. v1 launch firmware (ZMK) — current state and work package

Repo seed: `zmk-config/` — shield `vessel` on `seeeduino_xiao_ble`, GitHub Actions build,
badjeff `zmk-pmw3610-driver` module. Working: PMW3610 pointer (SPI1, MOT D7, CPI 600),
halls as `alps,ec11` steps=24 (bound to volume as placeholder), deep sleep 30 min, BLE +8 dBm.

**CRITICAL: the config predates the cap-touch pivot.** D0/D1 are still direct-GPIO buttons
(`kscan_direct` + 2-key keymap). The v1 work package:

1. Delete `kscan_direct` and the 2-key keymap (D0/D1 now belong to CAP1188).
2. Bring up CAP1188: Zephyr `microchip,cap12xx` input driver on I2C0 @0x29, INT on D0;
   thin listener mapping press/release → ZMK behaviors. Fallback if the driver fights
   ZMK's input pipeline: small custom module (ALERT pin + status registers).
3. Zone map per §3 (CS1→LCLK … CS6→PTT-as-dictation-hotkey-hold).
4. Slider v1: CS7=scroll-left/vol-down, CS8=scroll-right/vol-up, both=mute.
5. Haptic module (no ZMK driver exists): DRV2605L init + auto-cal at boot (persist),
   library effect per CS press; touch→haptic <10 ms.
6. Glow v1 on D6 (`worldsemi,ws2812-spi` compatible works for SK6812): amber idle low
   duty, zone flash on touch, breathe on proximity wake, all-off in sleep. No animations.
7. Wake: IMU wake-on-motion + CAP1188 standby-cycled proximity; glow breathe masks BLE
   reconnect (1–2 s).
8. Profiles: ZMK 5-profile BLE; gesture = touch-hold slider + crown spin steps profiles
   (haptic tick per step, LED0–4 blink index); long-hold = BT_CLR.
9. Rebind crown to true wheel scroll (`&msc`); slider owns volume.
10. Stretch: `vessel_dongle` shield (2nd XIAO as BLE-central→USB-HID bridge).

**v1 definition of done:** pair to two hosts + gesture-switch between them; ball/ring/
6 zones/slider all functional with per-zone haptics; approach glow; soft-sleep <50 µA
target, active <5 mA avg with glow at duty (30-day standby on 450 mAh); survives sleeve
on/off without retune (auto-recal on wake).

## 5. Phase 2 — custom Zephyr (product vision)

Drop from ZMK to raw Zephyr when audio + custom BLE + sensor fusion are in scope.
Architecture: input service (PMW3610/CAP/halls/IMU) → interaction engine (gestures,
slider interpolation, chords) → deterministic policy/rules engine (per-host+per-app
profiles, counters — NOT an LLM) → outputs (glow scenes, haptic grammar, I2S clip
player) + transports (BLE HID+BAS+custom ORBit GATT config service; USB composite
HID+MSC+CDC, later UAC; littlefs on QSPI).

Commitments: continuous slider w/ LED position feedback; precision mode (hold zone →
CPI÷4) and jog (hold + crown); voice clip library in QSPI (nRF cannot TTS — arbitrary
speech is host-rendered over USB Audio when docked); consistent haptic vocabulary;
IMU lift-to-lock / double-tap / tilt-hold host switch; config-as-files over opt-in USB
MSC (hold CS6 while plugging — BadUSB-perception mitigation); ORBit GATT service
mirroring config; TinyML capped at exactly two models (PDM keyword spotting, IMU
gesture classification; TFLite-Micro).

## 6. Phase 3+ / adjacent

- Mission deck: powered SBC dock (Pi Zero 2 W class) scripting the device over USB
  composite + versioned CDC command channel — keep descriptors clean now.
- DFU: stock UF2 (double-tap reset) for v1 → MCUboot signed BLE DFU in Phase 2
  (sleeve-off access must never be required for updates).
- Absolute "point-at-machine" switching: impossible on 6-axis (no heading ref) — needs
  9-axis respin or BLE AoA. Deferred; tilt-relative covers it.
- Battery: voltage-curve fuel gauge → BLE BAS + crown-hold charge meter on LED0–5.
- Manufacturing test mode: hidden build target exercising every subsystem (LED walk,
  haptic sweep, tone, sensor dumps over CDC). Build it in Phase 1.

## 7. Working agreements

- Bring-up order (hardware-risk order): pointer direction → ring steps → CAP sensitivity
  (sleeve fitted) → haptic autocal → glow chain → sleep/wake → profiles → audio.
- Keep the ZMK shield for v1; Phase 2 lives in a sibling Zephyr app sharing devicetree
  fragments.
- Identity guardrails: amber-family light only, no RGB theatrics; subtle haptics; sound
  off by default; every feature works sleeve-on.
- The CAD is parametric Python — if firmware needs a physical change (LED position,
  access hole), it is a one-line edit and a reprint. Ask; don't work around.
