# ORBit — Hardware Validation Checklist

Every value in the v1 firmware that can only be confirmed on the physical device
lands here, with the **exact expected behavior** and **how to adjust** it. The
firmware ships a best-documented default plus a marked tuning point (Kconfig
option or devicetree property) for each; this file is where a human closes the
loop with hardware in hand.

- **No hardware was in the loop** when this firmware was written. Treat every
  unchecked box as unverified, not as broken.
- Tune in **bring-up order** (spec §7, hardware-risk order): pointer direction →
  ring steps → CAP sensitivity (sleeve fitted) → haptic autocal → glow chain →
  sleep/wake → profiles → audio.
- **Cap-sense and glow diffusion must be tuned with the TPU sleeve fitted** — the
  dielectric stack roughly halves raw touch counts vs. a bare bench (spec §3).
- The manufacturing **test mode** build (spec §6: LED walk, haptic sweep, tone,
  sensor dumps over CDC) is the intended harness for running this checklist. It
  is built in Phase 1; see its section below once it lands.

This checklist grows as each work-package PR merges. Items are added by the PR
that introduces the tunable.

---

## Legend
- **Observe** — what to do / watch on the device.
- **Expected** — the pass condition.
- **Tune** — the firmware knob to change if it's wrong, and where it lives.

---

## 1. Trackball pointer direction & CPI  _(seed; PMW3610)_
- [ ] **Observe:** move the ball up/down/left/right; watch the cursor.
- **Expected:** cursor tracks 1:1 in the same direction on all four axes.
- **Tune:** the board mounts **component-side down, long axis = device x**
  (spec §3), so at least one invert/swap is likely needed. Uncomment/adjust in
  `zmk-config/config/boards/shields/vessel/vessel.overlay` on the `trackball`
  node: `swap-xy;`, `invert-x;`, `invert-y;`.
- [ ] **Observe:** general tracking speed/feel at the default **CPI 600**.
- **Expected:** comfortable desktop tracking; no skipping.
- **Tune:** `cpi = <600>;` on the same node (datasheet Z = 2.2–2.6 mm; lens ref
  plane sits at Z = 2.4 to the ball — mechanical, not firmware).
- **Note:** the PMW3610 runs on **SPIM3** (not SPIM1) — SPIM1 and the CAP1188's
  I²C (TWIM1) are the same nRF52840 instance and can't coexist. Same pins
  (D8/D9/D10); if the pointer is dead after the item-2 change, suspect the SPI3
  move first.

## 2. Scroll ring (hall quadrature) step feel  _(seed)_
- [ ] **Observe:** spin the crown; count detents vs. scroll/volume steps.
- **Expected:** smooth, continuous stepping; direction matches spin.
- **Tune:** `steps = <24>;` on the `scroll` (`alps,ec11`) node in the overlay.
  > Note: spec §3/§4 say **24 edges/rev** (authoritative); the project README
  > §5.1 says `steps = 6`. Per precedence the firmware uses 24. If detents feel
  > too coarse/fine on-device, this is the knob. If direction is reversed, swap
  > `a-gpios`/`b-gpios` (D2/D3).

## 3. CAP1188 touch bring-up (sleeve fitted)  _(PR item 2; `microchip,cap12xx`)_
Tune with the **TPU sleeve fitted** — the 1.0 mm TPU + 1.2 mm shell stack roughly
halves raw counts vs. bare bench (spec §3).
- [ ] **Observe:** touch the three click caps. **Expected (item 2 scope):** CS1 =
  left click, CS2 = right click, CS3 = middle click. CS4–CS8 do nothing yet
  (they're `&none` until items 3/4).
- [ ] **Observe:** ALERT/interrupt path — a touch registers promptly (spec target
  <10 ms). **Tune:** `int-gpios` on the `cap1188@29` node (D0, active-low,
  internal pull-up). If nothing registers, check the ALERT pull-up and that
  RESET (D1) is tied high (spec §2).
- [ ] **Observe:** no phantom/adjacent-zone triggers. **Expected:** each cap fires
  only its own zone.
- ⚠️ **KNOWN TUNING GAP — read before bench tuning.** The stock `microchip,cap12xx`
  driver exposes **no sensitivity/gain/threshold devicetree property** (only
  `int-gpios`, `repeat`, `poll-interval-ms`, `input-codes`). It runs the chip's
  **default** sensitivity. So if, with the sleeve on, touches are too weak
  (missed) or too hot (false triggers), there is **no firmware knob** in this
  driver. Options, in order: (a) increase electrode foil area / improve the
  dielectric contact (hardware, spec §7 allows a one-line CAD change); (b) toggle
  `repeat` and `poll-interval-ms`; (c) patch/fork the driver to write the CAP1188
  SENSITIVITY (0x1F) and averaging (0x24) registers at init, exposed as a Kconfig
  option; (d) fall back to the custom module (spec §4.2). Item 2 ships default
  sensitivity; capture the observed behavior here and pick an option if needed.
- [ ] **Observe:** I²C bus health with both devices — CAP1188 @0x29 and (after
  item 5) DRV2605L @0x5A share `&i2c1` (D4/D5). **Expected:** both enumerate; no
  bus lockups.

## 4. Touch zone mapping CS4/CS5/CS6  _(PR item 3)_
- [ ] **Observe:** in a web browser, tap CS4, then CS5.
- **Expected:** CS4 = browser **back**, CS5 = browser **forward** (they send
  mouse buttons 4/5, the standard back/fwd buttons — most OSes/browsers honor
  them out of the box; some Linux setups need `imwheel` or equivalent).
- **Tune:** `bindings` indices 3/4 in `cap_zone_behaviors`
  (`zmk-config/config/vessel.keymap`).
- [ ] **Observe:** hold CS6, speak, release. **Expected:** the host's
  dictation/voice input engages while held. CS6 holds **`C_VOICE_COMMAND`**
  (HID consumer 0xCF — the mic key on modern keyboards) for as long as the
  finger stays on the zone:
  - **macOS:** 0xCF is the dictation key. Default dictation is *tap to start,
    tap again to stop* — a quick tap of CS6 toggles it. For true
    press-and-hold PTT, hold-to-talk dictation tools (or newer macOS
    hold-the-mic-key behavior) use the hold directly.
  - **Windows 11:** 0xCF opens voice typing. **Windows 10 / not honored:**
    swap the binding for the explicit chord `&kp LG(H)` (Win+H).
  - **iOS/Android:** 0xCF summons the assistant (hold = hold-to-talk).
- **Tune:** `bindings` index 5 in `cap_zone_behaviors`. This is inherently
  **per-host** — pick the binding for your primary OS; per-profile PTT
  variants would need a layer/profile-conditional setup (note for item 8).
- [ ] **Observe:** brush a finger across adjacent zones (CS4→CS5, CS1→CS3).
- **Expected:** each zone fires once, no double-fires at boundaries. If zones
  bleed together, revisit §3's sensitivity options (sleeve fitted).

## 5. Slider v1: CS7/CS8 taps + both = mute  _(PR item 4)_
Spec §3: "v1: half-strip taps (vol/scroll, both=mute)". The firmware picks
**volume** (item 9 later gives the crown true wheel scroll and says "slider
owns volume"). Chording is done by the repo-local `orbit,input-processor-chord`
module (`zmk-config/modules/orbit/`) — ZMK combos only work on kscan positions,
not input events, so this is custom code: **bench-test it deliberately.**
- [ ] **Observe:** tap CS7, tap CS8. **Expected:** volume down / volume up, one
  step per tap, arriving up to ~75 ms after the tap (the chord window).
- [ ] **Observe:** hold CS7 or CS8. **Expected:** most hosts auto-repeat a held
  volume key. If yours doesn't, repeat-on-hold needs the cap12xx `repeat`
  property (see §3) — capture behavior here first.
- [ ] **Observe:** press both pads together (flat two-finger tap). **Expected:**
  one **mute** toggle; NO volume step before it. Release and repeat — unmute,
  again no stray volume steps.
- [ ] **Observe:** deliberately sloppy chords (one finger lands ~50–100 ms
  after the other). **Expected:** inside the window = mute; outside = two
  volume steps. If real two-finger taps often land as volume steps, widen the
  window; if single taps feel laggy, narrow it.
- **Tune:** `timeout-ms` on the `slider_chord` node in `vessel.overlay`
  (default 75). It is both the chord window and the worst-case single-tap
  latency — one knob, two effects.
- [ ] **Observe:** rapid alternating taps CS7-CS8-CS7… **Expected:** clean
  volume steps, no missed or stuck events. (The processor's pathological-case
  guarantee: rare degenerate sequences may *miss* a chord, but can never leave
  a key stuck.)

## 6. DRV2605L haptics: LRA auto-cal + per-zone feel  _(PR item 5)_
Playback uses the stock Zephyr `ti,drv2605` driver; boot auto-cal/persist and
the per-zone map are the repo-local `orbit,haptic-feedback` glue
(`zmk-config/modules/orbit/src/haptic_feedback.c`).
- [ ] **Before first power-up:** set `vib-rated-mv` / `vib-overdrive-mv` on the
  `drv2605@5a` node (`vessel.overlay`) from the actual LRA datasheet. Shipped
  defaults (1800/2500 mV) suit a generic Ø10×4 LRA; too high can cook a small
  motor, too low feels mushy.
- [ ] **Observe (first boot only):** ~0.8 s after power-on the LRA buzzes for
  up to ~1 s — that's auto-cal. Log line `LRA auto-cal OK (...) persisted`.
  **Expected:** later boots are silent with `LRA cal restored (...)` instead
  (results persist in settings under `orbit/lracal`).
  - If the log says `auto-cal FAILED` or `timed out`: check LRA wiring; the
    firmware falls back to open-loop drive (weaker but functional). Consider
    setting `lra-drive-time` on the `haptic_feedback` node = LRA half-period
    (`((1000/f_hz)/2 − 0.5)/0.1`, e.g. 24 for 170 Hz) and re-running cal
    (delete the setting or reflash with erased settings partition).
- [ ] **Observe:** touch each zone. **Expected feel** (spec §3 identity —
  effect IDs are the `effects` array on `haptic_feedback`, TI ROM library):
  CS1/CS2 sharp click (4), CS3 lighter click (5), CS4/CS5 soft tick (26),
  CS6 double-tick (10), CS7/CS8 micro-tick (24). Buzz should feel *subtle* —
  identity guardrail, not phone-vibrate.
- [ ] **Observe:** haptic latency — touch-to-buzz should read as instant
  (<10 ms; it fires on raw pad contact, before the slider chord window).
- [ ] **Observe:** single slider tap = exactly ONE tick (not a double ~75 ms
  apart). If doubled, `dedup-ms` (90) has been set below the chord window.
  A two-finger mute chord = two near-simultaneous ticks (one per pad) — this
  is expected v1 behavior.
- [ ] **Observe:** I²C bus health — CAP1188 @0x29 and DRV2605L @0x5A share
  `&i2c1`; hammer zones while rolling the trackball, watch for bus errors in
  the log.
- **Power note (for item 7/DoD):** the DRV2605 is left out of standby while
  awake (~0.5 mA quiescent). Sleep/standby gating lands with item 7's power
  work (`PM_DEVICE` suspend already exists in the driver).

## 7. SK6812 glow chain: order, color, duty  _(PR item 6)_
Rendering is the repo-local `orbit,glow` engine (`modules/orbit/src/glow.c`)
on the stock Zephyr `worldsemi,ws2812-spi` driver (SPIM2, MOSI-only on D6).
Tune **with the TPU sleeve fitted** — the lenses diffuse and dim the dots.
- [ ] **Observe:** at boot, all 8 dots come up faint amber (idle scene).
  **Expected:** color reads as warm amber (#FFB457 family) through the
  sleeve, no green/blue tint. If tinted, the strip's color order is off —
  fix `color-mapping` on the `ws2812@0` node (SK6812 variants exist in GRB
  and RGB).
- [ ] **Observe:** touch CS4 (west-most zone). **Expected:** the WEST-most
  dot (LED0) flashes brighter for ~180 ms. Walk all zones: CS4→0, CS5→1,
  CS6→2, CS1→3, CS3→4, CS2→5, CS7→6, CS8→7. If the pattern is mirrored, the
  chain was routed east→west — reverse `zone-leds` on the `glow` node.
- [ ] **Observe:** idle duty and flash brightness through the sleeve.
  **Tune:** `idle-brightness` (8%) and `flash-brightness` (30%) on the
  `glow` node. **The 30% is a spec cap on battery, not a starting point** —
  go down, not up. All-LED current at 30% ≈ tens of mA; check the power
  budget (§12) before raising anything.
- [ ] **Observe:** leave the device untouched for the ZMK idle timeout
  (30 s default). **Expected:** glow fades to fully off (quiet-desk
  identity + battery). Touch → idle scene returns instantly.
- [ ] **Observe:** after deep sleep (30 min) or power-off, ALL LEDs are
  dark. WS2812s latch their last frame — if any dot stays lit in sleep, the
  sleep-blank path failed; check the activity listener log.
- [ ] **Observe:** first LED glitches/flickers at low battery. SK6812 on a
  battery-voltage rail with 3.3 V logic is in-margin per spec §2, but
  marginal near full charge (4.2 V rail wants ≥ 2.9 V data-high). If seen,
  it's the hardware series-resistor/level topic from the spec, not firmware.
- **Note:** the breathe scene exists (`orbit_glow_breathe()`) but nothing
  triggers it until item 7 wires it to proximity wake.

## 8. Wake: IMU wake-on-motion + CAP proximity  _(PR item 7)_
Deep sleep is nRF System OFF; **waking is a full reboot** (expect the boot
glow breathe, then BLE reconnect under it — that's the intended masking).
Wake sources are armed by `orbit,wake` (`modules/orbit/src/wake.c`) on the
way down. All register values are AN4987/datasheet starting points — this
section is the tuning pass.
- [ ] **Observe:** let the device deep-sleep (30 min idle, or shorten
  `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT` for the bench). Nudge the desk vs. pick the
  unit up. **Expected:** desk vibrations do NOT wake it; picking it up does.
  **Tune:** `wake-threshold` on the `wake@6a` node (steps of 31.25 mg at
  2 g full scale; 2 = 62.5 mg). Raise for fewer false wakes.
- [ ] **Observe:** with the unit asleep, bring a hand near the slider edge
  (CS7/CS8 are ganged as the proximity antenna, sleeve fitted).
  **Expected:** approach wakes it before contact. **Tune:**
  `proximity-sensitivity` (0–7) and `proximity-threshold` on `wake@6a`.
  Too sensitive self-wakes from drift — verify it stays asleep overnight.
- [ ] **Observe:** on every wake/boot: glow breathes for ~2 s
  (`breathe-ms`) while BLE reconnects; typing/pointing works as soon as
  reconnect completes. If reconnect routinely outlives the breathe, raise
  `breathe-ms`.
- [ ] **Measure (DoD):** soft-sleep current, target **< 50 µA**. Budget:
  nRF System OFF ~1 µA + IMU low-power 26 Hz ~5 µA + CAP1188 standby
  (dominant term, duty-cycled) + DRV2605/LED quiescents. If over budget the
  knob is the CAP1188 STANDBY_CONFIG cycle time (reg 0x41 — not yet exposed
  in DT; raw default 0x39 ≈ 70 ms cycles) and, if needed, dropping the
  proximity wake entirely (motion-only wake still meets the spec's intent).
- [ ] **Observe:** wake works with the device on USB too (System OFF + USB
  is a corner: charger keeps the rail up — confirm no boot loop).

## 9. BLE profiles + gesture switch  _(PR item 8)_
Gesture: **hold a slider pad (~300 ms) + spin the crown** = step through the
5 BLE profiles; each step ticks the LRA and blinks the profile's index on
LED0–4. **Hold one pad alone for 5 s = `BT_CLR`** (clears the ACTIVE
profile's bond — deliberate friction, see below).
- [ ] **Observe:** pair two hosts on profiles 0/1. Hold CS7 (or CS8), spin
  the crown one detent. **Expected:** haptic tick, one LED of LED0–4 lights
  ~0.9 s showing the new index, HID output moves to the other host within a
  second or two. Spin back — returns.
- [ ] **Observe:** quick slider taps still do volume; the hold must engage
  before the crown affects profiles (base layer keeps the crown on volume).
  **Tune:** `tapping-term-ms` on the `sht` hold-tap in `vessel.keymap`
  (300): shorter = profile layer engages faster but quick volume taps risk
  reading as holds.
- [ ] **Observe:** hold one pad 5 s without spinning. **Expected:** at 5 s
  the active profile's bond clears (host disconnects; profile is ready to
  re-pair). The 5 s is `long-hold-ms` on `slider_chord`
  (`vessel.overlay`) — it is intentionally far above gesture time; if it
  ever fires during normal profile switching, RAISE it (a surprise BT_CLR
  is the worst UX failure this device can produce).
  - Known interaction: the long-hold timer does NOT know about crown spins;
    a 5 s+ hold *while* switching will still fire BT_CLR at 5 s. Bench-check
    whether real usage ever holds that long mid-gesture.
- [ ] **Observe:** profile blinks also fire when the profile changes by any
  other path (e.g. `&bt` bindings added later, or a host-initiated switch) —
  the indicator listens to ZMK's profile event, not the gesture.

## 10. Crown wheel scroll  _(PR item 9)_
The crown is now a true mouse wheel (`&msc` per detent); volume lives on the
slider. The profile layer (§9) still overrides the crown while a slider pad
is held.
- [ ] **Observe:** spin the crown in a scrollable window. **Expected:** one
  wheel step per detent, spin-toward-you = scroll down. If inverted, swap
  `SCRL_DOWN`/`SCRL_UP` on `rot_scroll` in `vessel.keymap` — OR swap
  `a-gpios`/`b-gpios` in the overlay if §2's direction check also failed
  (fix the hardware sense once, not twice).
- [ ] **Observe:** scroll feel vs. detent count. 24 edges/rev = 24 wheel
  ticks/rev. If that reads too fast/slow on-device, the knobs are `steps`
  on the encoder node (§2) or a scaler processor later — capture the feel
  here first.
- [ ] **Observe:** volume still works from slider taps only (CS7 down /
  CS8 up, chord = mute); nothing else emits volume anymore.

## 11. Manufacturing test mode  _(spec §6)_
Flash the **`vessel-testmode-xiao_ble`** CI artifact (a separate uf2 — the
normal build never contains any of this). Open the USB CDC serial console
(it enumerates alongside HID; logs start ~3 s after boot).
- [ ] **Observe:** banner `=== ORBit MANUFACTURING TEST MODE ===`, then:
  - **LED walk** — one LED per second, 0→7, forever. Any dark position =
    that LED or its data-in joint.
  - **Haptic sweep** — one ROM effect per 8-second lap (`HAPTIC effect N`
    in the log). Note the IDs that feel right for §6's per-zone table.
  - **Touch dumps** — every zone press/release logs `TOUCH code=N`.
  - **Trackball** — roll it; per-second `TRACKBALL dx dy` summaries.
  - **IMU** — per-second raw accel; flip the unit and watch z change sign.
  - Crown detents are verified via host scroll (item 9), not the log —
    the encoder is a ZMK sensor, not an input device.
- **Not covered:** speaker tone — audio is Phase-2 scope (spec §5); there is
  no amp path in v1 firmware to exercise. Add a tone step when I2S lands.

<!--
## 12. Power budget                                   — DoD
     (soft-sleep < 50 µA; active < 5 mA avg w/ glow at duty; 30-day standby
      on 450 mAh; survives sleeve on/off without retune — auto-recal on wake)
      Measured via §8's sleep checks + §7's duty tuning; record numbers here.
-->
