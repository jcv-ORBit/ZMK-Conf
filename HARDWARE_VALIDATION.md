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

<!--
Items below are placeholders for the subsystems each numbered work-package PR
introduces. Each PR appends its concrete tuning points and pass criteria here.

## 4. Touch zone mapping (CS1..CS6)                   — PR #3 (item 3)
## 5. Slider (CS7/CS8)                                — PR #4 (item 4)
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
