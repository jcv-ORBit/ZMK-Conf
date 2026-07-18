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

<!--
Items below are placeholders for the subsystems each numbered work-package PR
introduces. Each PR appends its concrete tuning points and pass criteria here.
## 6. DRV2605L haptics: LRA auto-cal + per-zone feel  — PR #5 (item 5)
## 7. SK6812 glow chain: order, color, duty           — PR #6 (item 6)
## 8. Wake: IMU wake-on-motion + CAP proximity        — PR #7 (item 7)
## 9. BLE profiles + gesture switch                   — PR #8 (item 8)
## 10. Crown wheel scroll                             — PR #9 (item 9)
## 11. Manufacturing test mode                        — spec §6
## 12. Power budget                                   — DoD
     (soft-sleep < 50 µA; active < 5 mA avg w/ glow at duty; 30-day standby
      on 450 mAh; survives sleeve on/off without retune — auto-recal on wake)
-->
