# PCB TODO — PSU Analog Control Interface

Status check against `docs/Kicad/Temp Snapshot/1000W Controller/1000W Controller.kicad_pcb`:
the `PC`/`PV` analog control path (CN71) is **not yet on the board**. `A5`
(`PIN_PSU_PC_PWM`) is unconnected, and there is no `CN71`-mating connector
footprint anywhere in the design. Only the `Remote ON/OFF` path (`Remote1`,
`Therm_sw1`, `U7`) is built. Everything below the line is new work.

See [Power_Domains.md](Power_Domains.md) for the domain reasoning behind
these decisions.

## TODO

- [ ] Add a `CN71`-mating connector (HRS DF11-12DP-2DS or equivalent) to break out the UHP-1500's `PV`/`PC`/`GND-signal` pins.
- [ ] Add RC low-pass filter from `A5` to CN71 pin 2 (`PC`): 1 kΩ resistor (any tolerance, 1/8 W is plenty — current is ~3.3 mA max) + 10 µF **ceramic** (X7R, ≥6.3 V — `PC` only ever sees 0–3.3 V, so no need for an electrolytic) in series/shunt for a ~16 Hz corner. No op-amp/buffer — `PC` is high-impedance and, once `ISO_GND` is bonded to `GND-signal`, there's no isolation boundary left to cross on this path.
- [ ] Add a single-point bond from `ISO_GND` to CN71 pins 3/4 (`GND-signal`) — **not** to `TB3 -Vo` (the heavy-current output terminal; bonding there will corrupt the setpoint with IR drop under load). The filter capacitor's ground leg landing directly on the `GND-signal` wire *is* this bond — don't add a second connection anywhere else between `ISO_GND` and `GND-signal` (stray copper pour, shared shield, etc.), or it becomes a ground loop. Keep this return wire short and direct: it's the reference the PSU measures `PC` against, so resistance or noise picked up here shows up directly as current-setpoint error.
- [ ] Leave `PV` (CN71 pin 1) unpopulated/unused unless output-voltage trim is needed later — firmware only drives `PC`.
- [ ] Confirm `R4` (footprint `1-10M`, bridges `ISO_GND`↔primary `GND`) is populated with a specific value in that range and left in place — do not remove.
- [ ] **Do not** bond `GND-AUX`/`+12V-AUX` to `ISO_GND` or `GND-signal` — verify `U7` remains the only bridge across that boundary.
- [ ] If DC-OK ever gets wired (to `MISO`, per [Pinout.md](../Pinout.md)): it's Note2-isolated, referenced to `GND-AUX` — needs its own isolator (same pattern as `U7`), not a direct connection.
- [ ] Layout/DRC pass: verify creepage/clearance between the primary `+24V`/`GND` domain and the isolated `ISO_GND` domain is maintained wherever the new CN71 connector and RC filter land, given they're new copper on an existing isolated-domain boundary.
- [ ] Fix courtyard overlap between `C13` and `Feather_M4_CAN_Express1` (real placement conflict — see [PCB_Review_kicad-happy.md](PCB_Review_kicad-happy.md)).
- [ ] Add a ground/power plane between every pair of signal layers in the stackup — currently `F.Cu`/`In1.Cu`/`In2.Cu`/`B.Cu` are all signal-adjacent with no plane anywhere, a board-wide EMC/return-path gap.
- [ ] Move decoupling closer to `U7` (isolation optocoupler, currently >3mm away) and `U3` (isolated-domain LDO).
- [ ] `24V_5V_Buck_ISO1` sits 0.42mm from the board edge — worth a second look given it's the actual power-isolation boundary component; give it a populated `Value` field too so future automated review recognizes it as an isolator.
- [ ] Backfill MPNs onto the BOM (we already have real part numbers for several: `SPB09W8-05`, `R-78E-0.5`, `LD1117V`, `ADuM1201AR`) — currently 0/20 parts have one, which blocks thermal analysis and datasheet-verified review.

## Component List

### Already on the board (relevant to this interface)

| Ref | Part | Role |
| :--- | :--- | :--- |
| `24V_5V_Buck_ISO1` | SPB09W8-05 isolated DC/DC | Primary `+24V`/`GND` → isolated `+ISO_5V`/`ISO_GND` |
| `U2` | RECOM R-78E-0.5 | `+24V` → `+3.3V` (primary domain) |
| `U3` | LD1117V | `+ISO_5V` → `+ISO_3.3V` (isolated domain) |
| `U1`, `U4`, `U5`, `U6` | ADuM1201AR (x4) | Fan/pump PWM & tach isolation |
| `U7` | DC optocoupler (photo-NPN) | Isolates Remote ON/OFF switching (`ISO_GND` ↔ `GND-AUX`/`/12V_AUX`) |
| `R4` | 1–10 MΩ | Bleed resistor, `ISO_GND` ↔ primary `GND` |
| `R12` | 470 Ω | Current-limit for `U7`'s LED side |
| `Remote1`, `Therm_sw1` | Terminal + switch | Remote ON/OFF loop to PSU |

### Newly needed

| Part | Purpose | Notes |
| :--- | :--- | :--- |
| CN71 mating connector | Break out `PV`/`PC`/`GND-signal` from the UHP-1500 | HRS DF11-12DP-2DS or equivalent (per UHP-1500 spec) |
| Resistor, ~1 kΩ | RC filter, `A5` → `PC` | Value not yet finalized against actual PWM frequency |
| Capacitor, ~10 µF | RC filter, `A5` → `PC` | Same |
| Wire/0 Ω link | `ISO_GND` ↔ CN71 pin 3/4 (`GND-signal`) | Single-point bond, not a copper pour tie |

No new isolator, op-amp, or isolation amplifier is needed for this interface.
