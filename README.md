# ORBit — capacitive-touch trackball bar · Build Guide (v7 / cap-touch)

Low bar trackball on the XIAO nRF52840 **Sense Plus** + PMW3610 stack.
Bearing cup, magnet scroll ring, haptics, and — new in this revision —
**capacitive touch buttons (CAP1188)** replacing every mechanical switch.
Footprint: 135.2 × 34.7 mm, unchanged. This README supersedes the v6 one;
the "Vessel — Build & Wiring Guide" artifact remains valid for assembly
steps A–C and E–H (its section D "Switches" is obsolete, see §4 here).

## What changed in the cap-touch revision

1. **All 6 micro-switches, the floor pockets, and the carrier bridge are
   GONE.** Buttons are now touch zones on the shell, sensed by a CAP1188
   8-channel module on the same I²C bus as the DRV2605L haptics.
2. **New tray: `ORBit_bottom_tray_captouch.3mf`** (drop-in — same walls,
   towers, cup, collar, battery bay, XIAO rails, LRA slot, hall towers):
   - switch pockets and carrier ledges removed; right floor is now a
     **CAP1188 bay**: 40.6 × 18.6 mm cavity (fits a 40 × 18 × 4 module),
     x 22.0–62.6, y ±9.3, 3 mm retaining fence, recessed 1.1 mm into the
     east wall, finger gaps mid-span for removal, wire notch at the west
     end pointing at the DRV/XIAO, two 0.6 mm ribs so solder joints
     don't rock the board.
   - the EC11 recess ribs (gear-route leftovers) are also deleted.
3. **Obsolete files** (keep for reference or delete):
   `ORBit_switch_carrier_left.3mf`, old `ORBit_bottom_tray.3mf`.
4. **Haptic click is now core, not optional.** With no mechanical
   tactility, the DRV2605L fires a click effect on every touch event.
5. **Audio is now provisioned** (piezo, or MAX98357A I²S amp + micro
   speaker on the free EXT pads) so the hardware is ready for the Phase-2
   voice assistant with no respin. See §1 BOM, §3 pin map, §5.6 roadmap.

### Why the device height is unchanged
The ball height is locked by PMW3610 optics — ball bottom ≈ z11 over the
lens at ≈ z9.2. The 6 mm 602040 cell sits floor (z2) to z8, well **under
the z16.2 wall rim**, as do the audio mounts (top z ≈ 9) and CAP bay — all
inside the existing envelope. Any real *slimming* (~2 mm) would come from
the tray-wall / shell-skirt overlap, but the shell, sleeve, ring, and
crown are a dialed-in mated set — a coordinated shell-source pass, not a
tray change.

---

## 1. Sourcing BOM (current build)

### Electronics
- **Seeed XIAO nRF52840 Sense Plus** ×2 (one device, one 2.4 G dongle)
- **PMW3610 breakout + lens** ×1 — mounts to `ORBit_sensor_sled`
- **CAP1188 8-key capacitive touch module** (~40 × 18 × 4 mm) ×1 — NEW
- **DRV2605L haptic driver** ×1 + **coin LRA** ×1 (LRA slot under XIAO)
- **Hall latches ×2** (scroll ring quadrature). TLE4946-2L or any
  **bipolar hall-effect LATCH** in a TO-92-style leaded package that
  runs at 3.3 V — easy subs: Honeywell **SS460S**, Allegro **A1221LUA**;
  TI **DRV5012** if you're OK with SMD (lowest power). Must be a
  *latch* (flips on alternating N/S), NOT a unipolar/omnipolar switch.
  Open-collector/open-drain outputs need the 10 kΩ pull-ups to 3V3
  (§ assembly artifact, step C).
- **Ø3×2 N52 discs** ×12 (ring) + ×8 (shell mount, 4 pairs)
- **LiPo NST 602040, 450 mAh / 1.67 Wh, 3.7 V** ×1 (on hand, **protected**)
  — 6 × 20 × 40 mm; the `_audio` tray pocket is sized for it. The chosen
  cell: already has its protection board (no add-on, no live-cell
  soldering), standard 3.7 V so the XIAO charges it **fully** at 4.2 V,
  1.5× the original runtime, and only 6 mm thick so it clears the rim with
  room to spare while adding moderate heft. *(Alternatives that also fit:
  TW312540 300 mAh thin cell; or the 122134 1000 mAh HV brute — but that
  one is unprotected + HV, so it needs an added PCM and only partly
  charges.)*
- **Slide switch** SS-13D07 ×1 — hard battery disconnect (shipping/storage/
  failsafe), inline on B+. Day-to-day power is IMU wake + soft-sleep;
  you'll rarely touch it. Shelf retained in the tray.
- **25 mm trackball** · **MR63ZZ bearings** ×3 + 3 mm filament axles
- Copper foil tape (~10 mm wide) + 30 AWG wire — touch electrodes
- **SK6812MINI-E addressable LEDs ×16** (+ spares; buy a strip/bag):
  6–8 halo ring + 6 zone windows + 2 slider — SK6812 runs on battery
  voltage, unlike 5 V WS2812. Plus `ORBit_led_diffuser`; translucent
  **smoke TPU** for the sleeve (it is the diffuser for all windows)

### Audio (voice feedback / chimes — provisions the assistant phasing)
Two tiers; wire whichever you want now — both fit existing void space, **no
new 3D files**. See §5.6 for what firmware drives them.
- **Tier 1 — piezo transducer** (chimes/alerts only): a Ø20–27 mm bare
  piezo disc, 0.5 mm thick (e.g. **CUI CPT-2065** or any Ø27 element).
  One PWM pin, no amp. Bonds flat to the shell roof interior; the shell
  becomes the diaphragm and radiates through the glow slots + seams.
- **Tier 2 — real voice** (recommended for the assistant): **MAX98357A**
  I²S DAC+class-D amp breakout (~16 × 13 mm) + one **8 Ω micro speaker**
  (15 mm round, ~4 mm thick — **CUI CMS-15113** / **PUI AS01508AO-3-R**;
  or an 11 × 15 mm rectangular cellphone speaker). The nRF52840 has **no
  DAC**, so this amp (or the piezo) is mandatory for any sound.
  - Wire both if unsure: the MAX98357A path supersedes the piezo. You can
    populate Tier 2 later and leave the pins unused until Phase-2 firmware.

### Fasteners
- M2×8 self-tapping ×2 (sled → posts) · M2×5 self-tapping ×4 (PMW3610 → sled)

## 2. Print plan
PETG, 0.2 mm layers. Parts: `ORBit_bottom_tray_captouch_audio`,
`ORBit_top_shell_captouch_glow`, `ORBit_sensor_sled`, `ORBit_crown` (redesigned),
`ORBit_scroll_ring_magnet`, `ORBit_base_skate`, `ORBit_tpu_top_sleeve` (TPU),
`ORBit_tpu_pad` (TPU), dongle case, tilt wedges/sidecar as desired.
Tray upright with paint-on supports only inside the sensor bay; shell
top-face-down on textured PEI; the CAP bay prints support-free (the 1.1 mm
wall recess bridges).

## 3. Wiring (XIAO Sense Plus — updated pin map)

| Pin | Net | Connect to |
|---|---|---|
| D0 | TOUCH_INT | CAP1188 ALERT/INT (was BTN_M1) |
| D1 | TOUCH_RST | CAP1188 RESET — optional, else tie high (was BTN_M2) |
| D2 / D3 | SCROLL_A/B | hall latch 1 / 2 OUT |
| D4 / D5 | SDA / SCL | **shared I²C bus: DRV2605L + CAP1188** |
| D6 | LED_DIN | SK6812 DIN (330–470 Ω in series) |
| D7–D10 | MOT / SCK / nCS / SDIO | PMW3610 (3-wire SPI) |
| 3V3 / GND | rails | PMW3610, DRV (+EN), CAP1188, halls, amp, pull-ups |
| B+ / B− | LiPo | slide switch inline on B+ (or EXT6 soft-off) |
| CS1–CS8 | touch zones | 30 AWG from CAP1188 to copper-foil electrodes |
| EXT1 | I2S_BCLK | MAX98357A BCLK (Tier-2 audio) |
| EXT2 | I2S_LRC | MAX98357A LRC / word-select |
| EXT3 | I2S_DIN | MAX98357A DIN |
| EXT4 | AMP_SD | MAX98357A SD (mute / power-save; tie high if unused) |
| —or— EXT1 | PIEZO | Tier-1 piezo (single PWM pin; other leg → GND) |

All D0–D10 through-holes are full; **audio lives on the 9 free EXT
castellated pads** (freed when the switch wiring was deleted). I²C
addresses don't collide (DRV2605L 0x5A, CAP1188 0x29 default). The
MAX98357A takes 3V3 (works to ~2.5 V, fine on battery); OUT± → 8 Ω
speaker. GAIN pin sets level — leave floating for 9 dB or strap per its
datasheet.

### Electrode layout (under the shell roof / button caps)
Cut ~10 × 14 mm copper foil pads, stick to the underside of each shell
button cap (the flexure caps stay — they're now cosmetic touch landmarks),
solder a 30 AWG lead to each, route down to the CAP1188 bay. Keep pads
≥4 mm apart. Suggested map: CS1 left-click · CS2 right-click · CS3 middle
· CS4 back · CS5 forward · CS6 push-to-talk · **CS7+CS8 swipe strip**
along the front roof edge (two elongated pads, interpolate for a slider)
— or gang CS7+CS8 as a proximity antenna (see §5).

## 4. Assembly deltas vs the Vessel artifact guide
Follow the artifact steps A–C and E–H unchanged. Replace step D with:
1. Drop the CAP1188 into the right-floor bay (fence retains it; ribs
   under the board). Wires exit the west notch toward the XIAO.
2. Foil electrodes on the shell (above), leads with ~20 mm slack so the
   shell still pops off.
3. Set touch sensitivity after closing up — the stack is shell (1.2 mm)
   + TPU sleeve; CAP1188 gain has plenty of range for that, tune
   SENSITIVITY and averaging registers with the sleeve fitted.

## 5. Firmware
### 5.1 Base (ZMK on nRF52840) — Phase 1
- Base: ZMK + community PMW3610 module; halls = quadrature encoder
  (`alps,ec11`, A/B on D2/D3, `steps = 6`).
- Touch: Zephyr ships a Microchip CAP12xx input driver — bind it on the
  I²C bus with INT on D0 and map 8 inputs to mouse buttons / behaviors
  (small devicetree + listener shim in ZMK).
- **Haptic click**: on any CS press event, fire DRV2605L effect 1
  (strong click); use a sharper/lighter effect per zone so left, right,
  and PTT each *feel* different. Fire on press, not release, <10 ms.
- PTT / talk-to-text: hold CS6 → send the OS dictation hotkey (easy
  path); onboard PDM mic streaming over USB/BLE audio is the advanced
  custom-firmware path.
- Wake: IMU wake-on-motion interrupt from soft-off + CAP1188 proximity
  wake (it can run in a low-power cycled mode and interrupt on approach).

### BLE profiles, pairing, device switching
- ZMK keeps **5 BLE profiles** and auto-reconnects to the last active one
  on wake (IMU wake-on-motion → reconnect in ~1–2 s; approach-glow masks it).
- **Switch hosts**: bind `&bt BT_SEL n` / `BT_NXT` to a combo —
  recommended gesture: *touch-hold the slider (CS7/8) + spin the crown*;
  each detent steps a profile, halo blinks the profile number in amber,
  haptic tick per step.
- **Pair a new device**: select an empty profile — the device advertises
  automatically and appears in the host's Bluetooth list. Long-hold the
  same gesture = `&bt BT_CLR` (forget current profile).
- **Dongle** (2nd XIAO): a BLE→USB HID receiver for hosts with no/locked
  Bluetooth or for lowest latency. It is optional and is NOT storage —
  it enumerates as a plain USB mouse. It occupies one of the 5 profiles;
  moving the dongle between machines is itself a host-switching story.

### Ambient glow architecture (dark slate / copper-bronze accent)
Goal: soft warm amber (logo palette) halo around the ball, a dot per
touch zone, and a light-line along the slider. Subtle, breathe-on-approach,
off in sleep.
- **Driver**: preferred = the existing D6 addressable chain — use
  **SK6812MINI** (happy at 3.3–3.7 V on battery, unlike 5 V WS2812):
  6–8 LEDs in a ring under the aperture boss + 1 under each button
  window + 2 under the slider pinstripe ≈ 10–12 LEDs, one pin, exact
  copper-amber set in firmware. Alternative: the CAP1188 has 8 LED
  driver pins with hardware touch-linking and breathe/pulse behaviors
  (zero firmware) — but only if your module breaks LED1–8 out; most
  generic ones don't. Check the header before counting on it.
- **Ball halo needs no cutting**: light fired upward under the aperture
  boss leaks through the existing shell-aperture / ring-skirt shadow gap.
- **Button + slider windows — CUT, in `ORBit_top_shell_captouch_glow.3mf`**:
  0.8 mm through-slots at |y| = 13.9 (a third "light seam" running just
  outboard of the twin pinstripes). Zone windows (6 mm long) on one roof
  edge at x = **−38, −27** (back / fwd), **25** (PTT), **44, 52, 60**
  (L / M / R click); an 18 mm **slider window at x 41–59** on the
  opposite edge, directly above the CAP1188 bay. The windows *define*
  the touch zones — stick each foil electrode under the roof beside its
  window, LED under the slot. The six switch **stems are deleted** from
  this shell (nothing to press anymore); the flexure caps and hinges are
  kept as tactile landmarks. Sleeve stays **uncut** — print it in
  translucent smoke TPU: sealed against dust, diffuses the glow,
  invisible until lit.
- **HiLetgo module L1–L8 pins**: use them on the bench (hardware
  touch-linking while tuning sensitivity), not in the final glow — the
  SK6812 chain does color, blink-counts, and animations that the L-pins
  can't.
- **Power**: amber at low duty, breathing only on proximity/touch, all
  off in soft-sleep — negligible against the 300 mAh cell with
  wake-on-motion.

### Audio placement — integral mounts in the `_audio` tray
The tray carries the Tier-2 audio mounts over the CAP1188 bay (the only
place with vertical void):
- **MAX98357A amp**: recessed shelf over the CAP bay's **west/inner end**
  (x 22.5–35, seat z 5.8), non-magnetic so its nearness to the hall
  towers is harmless. Wires drop out the open east side to the CAP bundle.
- **Speaker**: an up-firing corral over the CAP bay's **east end**
  (x 45–59, seat z 5.6), aimed at the 18 mm slider glow window as its
  acoustic port (through the smoke TPU). Placed as far from the scroll
  **hall latches** as the bay allows — nearest-corner gap ≈ 13 mm.
  ⚠ A speaker has a magnet: if you see phantom scroll, that coupling is
  why — add a thin steel shield washer behind the speaker, or shift it
  east a few mm. Use the **thin 11 × 15 × 3.5 mm** rectangular speaker
  here (fits the corral and the roof); a 15 mm round also drops in.
- **Assembly order**: wire the CAP1188 first, then set the amp and speaker
  into their mounts (they sit just above the module). Dry-fit before
  bonding.
- **Piezo alternative** (Tier 1): skip both mounts, bond a Ø27 disc to the
  shell roof — sound exits the glow slots. One PWM pin, no amp.
- **Power**: chimes are free; class-D voice is bursty (100s of mA peaks) —
  the 1000 mAh cell handles short prompts easily, not continuous playback.

### Onboard storage ("carry your config")
- The XIAO nRF52840 has **2 MB QSPI flash**; TinyUSB MSC can expose it
  as a small USB drive over USB-C (wired only — BLE cannot serve files).
- Recommended product shape: **the device carries its own keymap/macros/
  settings as editable text files** — plug into any machine, tweak, done,
  no companion app. Do NOT pitch it as a file-shuttle: 2 MB is tiny, and
  a pointing device that also enumerates as removable storage pattern-
  matches BadUSB to corporate IT. Gigabyte storage would need a microSD
  on spare EXT pads (SPI) — judged overdesign; skip.

### 5.6 Firmware roadmap — the adaptation assistant (phasing)
The hardware above is fully provisioned for a voice + haptic "device
adaptation assistant." Build it in phases so the firmware ambition never
blocks the physical product — the same PCB stack runs all three.

**Phase 1 — ship on ZMK (now).** §5.1–§5 above: pointer, touch buttons,
haptic click, glow, per-profile BLE switching, IMU wake. A real, usable
trackball today. Validate the object.

**Phase 2 — custom Zephyr assistant.** ZMK *is* Zephyr underneath; drop to
raw Zephyr when the assistant features need audio + custom BLE + sensor
fusion (ZMK's abstractions fight all three). Buildable, genuinely
distinctive:
- **Voice feedback = pre-rendered clip library in the 2 MB QSPI flash**,
  played over I²S (Tier-2 amp). A finite prompt set ("switched to
  laptop," "precision on," "hold to talk") — how earbuds/cameras do voice.
  The nRF **cannot** synthesize arbitrary speech on-device; arbitrary TTS
  is a host job (host renders → streams over **USB Audio Class**, which
  also carries the PDM mic → the puck becomes a little speakerphone).
- **Personalization = an adaptive rules/stats engine, not an LLM.** Counters,
  timers, per-app config recall — deterministic, tiny, debuggable. "Learns
  your favorite config per host and announces the switch (voice + haptic +
  glow)" is the shippable core.
- **TinyML where it earns it:** two small models only — keyword spotting
  (a few voice commands) and IMU **gesture** classification. TFLite-Micro /
  Edge Impulse territory on the M4F.
- **Device switching by gesture (relative, reliable):** tilt/rotate the
  puck one way → host 1, the other way → host 2. Uses tilt-relative-to-
  gravity; rock-solid with the 6-axis IMU.

**Phase 3 — research spikes (defer).**
- **True spatial "point at the device" switching** is NOT possible with the
  6-axis IMU: accel gives tilt-vs-gravity, gyro drifts, and there's **no
  magnetometer** for absolute heading. It needs a 9-axis board (+ calibration
  ritual) or BLE AoA silicon (nRF5340-class). Use relative gesture switching
  (Phase 2) until/unless a 9-axis respin happens.
- On-device behavioral "AI" beyond the rules engine → host/cloud, not the M4F.

## 6. Feature roadmap ("over the edge" list, all zero/near-zero BOM)
1. **Solid-state buttons + per-zone haptic click** — the headline. No
   switch wear, sealed top, tunable feel.
2. **Swipe strip** (spare CS7/CS8): horizontal scroll, volume, or tab
   switcher. A trackball with a touch slider is a genuine differentiator.
3. **Approach glow + ambient accent lighting**: proximity mode + the
   amber glow architecture above — halo, zone dots, slider line breathe
   before your hand lands; radio wakes at the same moment (hides BLE
   reconnect latency).
4. **Precision / jog mode**: touch-hold a zone while rolling the ball →
   CPI drops 4×; hold + ring spin = per-app jog dial.
5. **PTT / talk-to-text** via onboard mic (CS6 zone) — already planned.
6. **IMU gestures**: wake-on-motion (planned), lift-to-lock (cursor
   freezes when you reposition the unit), tilt-to-switch hosts,
   double-tap the shell as a hidden button.
7. **Voice + haptic assistant** (Phase 2, §5.6): announced host switches,
   spoken onboarding/coaching from a clip library, keyword commands.
8. Deliberately *not* included: touch typing zones, RGB theatrics,
   displays, on-device TTS, absolute spatial awareness — they'd dilute the
   "quiet desk object" identity or over-reach the M4F/6-axis hardware.

## 7. File manifest (current)
| File | Status |
|---|---|
| `ORBit_bottom_tray_captouch_audio.3mf` | **CURRENT tray** — cap-touch + 602040 (450 mAh protected) battery pocket + integral amp/speaker mounts |
| `ORBit_bottom_tray_captouch.3mf` | superseded (cap-touch, old 300 mAh bay, no audio mounts) |
| `ORBit_bottom_tray.3mf` | superseded (switch version) |
| `ORBit_top_shell_captouch_glow.3mf` | **current shell** — glow windows cut, switch stems removed |
| `ORBit_top_shell_detailed.3mf` | ⚠ DO NOT SLICE — contains 7 stacked copies plus a misplaced collar chimney and a solid Ø22 plug at the origin (WIP leftovers) that would block the ball bore |
| `ORBit_tpu_top_sleeve.stl` / `ORBit_crown.stl` | current finish parts |
| `ORBit_scroll_ring_magnet` / `ORBit_sensor_sled` / `ORBit_base_skate` | unchanged |
| `ORBit_switch_carrier_left.3mf` | **obsolete** |
| `ORBit_all_files.3mf` | re-plate: swap in the captouch tray, drop the carrier |
| dongle case, LED diffuser, tilt wedges, sidecar, TPU pad | unchanged |

**Audio needs no 3D files** — piezo bonds to the shell roof; Tier-2
speaker + MAX98357A occupy existing void (§ Audio placement). Both are
retrofittable at any time.
