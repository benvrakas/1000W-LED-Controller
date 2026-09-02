# Power & Isolation Domains

Sourced from `docs/Kicad/Temp Snapshot/1000W Controller/1000W Controller.kicad_sch`
and `.kicad_pcb` (schematic net names and symbol `lib_id`s). This isn't written
up anywhere else — see [Hardware.md](Hardware.md) for the parts list and
[Electronics_Architecture.md](Electronics_Architecture.md) for signal pinout.

## Domains

The board has three electrically separate power/ground domains:

| Domain | Rails | Ground | Powers |
| :--- | :--- | :--- | :--- |
| **Primary (fan/pump power)** | `+24V`, `+3.3V` (regulated) | `GND` | Rad Fans x3, Aux Fan, PSU Fan, Pump motor power; primary-side (fan-facing) pins of the PWM/tach isolators |
| **Isolated (logic)** | `+ISO_5V`, `+ISO_3.3V` | `ISO_GND` | Feather M4 CAN Express MCU, OLED display, rotary encoder; logic-side pins of the PWM/tach isolators |
| **PSU-referenced** | `/12V_AUX` | PSU `−V` | PSU CN71 Remote ON/OFF loop (`Remote1`) |

The **primary** and **isolated** domains only meet through two isolation
components (below). The **PSU-referenced** domain only meets the isolated
domain through the optocoupler on the Remote ON/OFF line.

## Isolation Components

| Ref | Part | Bridges | Role |
| :--- | :--- | :--- | :--- |
| `24V_5V_Buck_ISO1` | **SPB09W8-05** isolated DC/DC converter | `+24V`/`GND` (primary) → `+ISO_5V`/`ISO_GND` (isolated) | The galvanic isolation boundary for **power**. Generates the isolated logic-side supply from the primary 24V rail. |
| `U1`, `U4`, `U5`, `U6` | **ADuM1201AR** dual-channel digital isolator (x4, one per fan/pump channel: Rad Fan, PSU Fan, Pump, Aux Fan) | `+ISO_3.3V`/`ISO_GND` (logic side) ↔ `+3.3V`/`GND` (fan side) | Isolation boundary for **PWM/tach signals** — see [Pinout_Verification_Report.md](../Pinout_Verification_Report.md) for the per-channel pin mapping. |
| `U7` | Optocoupler, DC, photo-NPN output (schematic `lib_id: Isolator:Optocoupler_DC_PhotoNPN_AKEC`) | `ISO_GND` (MCU-driven) ↔ `/12V_AUX` + `/remote_on_off` (PSU domain) | Isolates the **PSU Remote ON/OFF** switching path. Not previously documented — [Pinout_Verification_Report.md](../Pinout_Verification_Report.md) only covers the PWM/tach isolators. |

## Regulators (non-isolating)

| Ref | Part | Conversion | Domain |
| :--- | :--- | :--- | :--- |
| `U2` | RECOM **R-78E-0.5** switching step-down (labeled `R-783.3-0.5` in schematic) | `+24V` → `+3.3V` | Primary (feeds the fan-side pins of `U1`/`U4`/`U5`/`U6`) |
| `U3` | **LD1117V** linear regulator | `+ISO_5V` → `+ISO_3.3V` | Isolated (feeds MCU/display/encoder and the logic-side pins of `U1`/`U4`/`U5`/`U6`) |

## Primary `+24V` Source

`24V1` (a 2-pin terminal block) is fed by a **Mean Well LRS-350-24** (350W,
24V/14.6A), mounted on the same AC mains branch as the UHP-1500. See
[Hardware.md](Hardware.md) for the datasheet.

## Driving the UHP-1500 `PC` Line Relative to `-V`

The UHP-1500 spec sheet is explicit about which CN71 signals are isolated
and which aren't (see [UHP-1500-spec.pdf](../Hardware/UHP-1500-spec.pdf) p.6):

* **`PV`/`PC` (pins 1/2): non-isolated**, referenced directly to `GND-signal`
  (CN71 pins 3/4 — a low-current Kelvin tap of `-Vo`, distinct from the
  heavy-current `TB3 -Vo` power terminal).
* **Remote ON/OFF, DC-OK, comms (pins 5/6/11/12): isolated**, referenced to
  a separate `GND-AUX` rail. This is why `U7` (the Remote ON/OFF optocoupler
  documented above) exists but no equivalent isolator exists for `PC` — the
  PSU itself doesn't isolate that pin, so nothing on our board does either.

Because `PC` is non-isolated, `AnalogPsuBackend`'s PWM → RC filter → `PC`
path only reads correctly if it shares a common reference with the PSU's
`-Vo`. The recommended way to do that: **bond this board's `ISO_GND` to CN71
pins 3/4 (`GND-signal`)** at a single point, at the RC filter's ground
return — never to `TB3 -Vo`, which carries the full LED return current and
will corrupt the setpoint with IR drop under load.

This is safe to do without forming a ground loop through the `+24V`/`LRS-350-24`
domain, because `ISO_GND` is already isolated from that domain by the
`SPB09W8-05` converter above — bonding it to the UHP-1500's `-Vo` only
changes what the already-floating isolated domain references, it doesn't
re-connect it to the primary 24V supply. Both PSUs' outputs float relative to
frame ground on their own (UHP-1500: O/P-FG withstand 1.25kVac; LRS-350-24:
O/P-FG withstand 0.5kVac), so sharing the AC mains branch doesn't couple
their DC domains either — only the deliberate `ISO_GND`-to-`GND-signal` bond
described here does.

This whole `PC`/`PV` interface is not yet built on the PCB — see
[PCB_TODO.md](PCB_TODO.md) for the outstanding work and parts needed.
