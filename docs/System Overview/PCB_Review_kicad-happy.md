# PCB Review — kicad-happy, 2026-08-15

Automated review of `docs/Kicad/Temp Snapshot/1000W Controller/` via the
`kicad-happy` toolkit (schematic + PCB analyzers, cross-domain analysis, EMC
pre-compliance, thermal). Full JSON output cached under that folder's
`analysis/2026-08-15_1946/`. 223 raw findings total — most are boilerplate or
expected at this design stage; this doc keeps only what's actionable.

**Caveat this review inherits from the tool's own rules:** the schematic has
**0/20 unique parts with an MPN assigned** (`SS-001`, `DS-001`). Every finding
below is a consistency check (does the design agree with itself) — none are
verified against a manufacturer datasheet. See "Sourcing gap" below.

**Second caveat:** this analyzed the `Temp Snapshot` copy — if your live
working copy has since diverged, some of this may already be stale. Worth
re-running once the CN71 interface from [PCB_TODO.md](PCB_TODO.md) is added.

## Confirms our manual analysis

The tool's isolation-barrier detector (`IB-DET`) independently found exactly
what we traced by hand in [Power_Domains.md](Power_Domains.md): 2 ground
domains (`GND`, `ISO_GND`) and 5 isolation components — `U1`, `U4`, `U5`,
`U6` (ADuM1201AR) and `U7` (the Remote ON/OFF optocoupler). Good cross-check.

One gap in the tool's detection, worth noting: it does **not** count
`24V_5V_Buck_ISO1` (the `SPB09W8-05` module) as an isolation component,
because its footprint has no populated `Value`/`lib_id` the pattern-matcher
recognizes — it only sees the 4 digital isolators + optocoupler. We know from
the raw netlist trace (this session, earlier) that it's the actual power
isolation boundary. Worth giving that footprint a proper `Value` field so
future automated review picks it up.

## Real findings, ranked

1. **Courtyard overlap: `C13` and `Feather_M4_CAN_Express1`** (`PM-001`,
   error) — 14.72mm² overlap. This is a physical placement conflict, not a
   style nit — parts will physically interfere at assembly. Move `C13` or
   adjust its courtyard.

2. **4-layer stackup has no plane between any pair of signal layers**
   (`SU-001` ×3, error) — `F.Cu`/`In1.Cu`, `In1.Cu`/`In2.Cu`, and
   `In2.Cu`/`B.Cu` are all adjacent signal-to-signal with no ground/power
   plane between them anywhere in the stackup. This is a board-wide EMC/
   return-path issue, not a local one — every signal on this board is missing
   a clean return path on at least one side. Given this board has 4 fan PWM
   channels crossing an isolation boundary, this is worth fixing before fab,
   not after.

3. **Decoupling cap too far from `U7`** (`DC-001`, error; reco: within
   2–3mm) — `U7` is the optocoupler at the Remote ON/OFF isolation boundary.
   Marginal decoupling right at an isolation crossing is exactly where you
   don't want it. `U3` (the isolated-domain LDO) has the same issue at
   `warning` severity — moderately far, fix if layout permits.

4. **`24V_5V_Buck_ISO1` is 0.42mm from the board edge** (`PM-002`, error;
   recommends ≥1.0mm) — this is the actual isolation-boundary component
   (the isolated DC/DC converter). Being this close to the board edge is
   worth a second look given it's also carrying the isolation-barrier
   creepage/clearance requirement `IB-DET` flagged generally (IEC 60664-1).
   `U3`, `Aux_Fan_1`, `Rad_Fan_1`, `Rad_Fan_3` also flagged at error severity
   for edge proximity (0.39–0.43mm); the `Feather_M4_CAN_Express1` courtyard
   also overhangs the board edge by 0.25mm.

   *Triage note:* several other `PM-002` hits (24V1, Display1, Encoder1,
   LED_TEMP1) are tagged by the tool itself as `info`/"by-design at board
   edge" — those are legitimate edge-mount connectors, not bugs. Only the
   `error`-severity ones above need a look.

## Sourcing gap (blocks deeper verification)

`SS-001`: 0 of 20 unique parts have an MPN. This is why the datasheet-driven
Deep Review pass (per-IC pin/power verification against manufacturer specs)
couldn't run, and why thermal analysis produced 0 findings — no MPN means no
package/θJA data to estimate junction temperature for `U2` (buck reg),
`U3` (LDO), or `24V_5V_Buck_ISO1`, all of which dissipate real power on this
board. Backfilling MPNs onto the BOM (we already have real part numbers for
several of these from this session's work — `SPB09W8-05`, `R-78E-0.5`,
`LD1117V`, `ADuM1201AR` — plus the datasheets already in `docs/Hardware/`)
would unlock thermal analysis and a real datasheet-verified review next pass.

## Not performed

- **SPICE simulation** — no simulator installed (`ngspice`/`ltspice`/`xyce`
  all absent from PATH). Would have validated the RC filter frequencies etc.
- **Deep per-IC datasheet review pass** — blocked by the sourcing gap above.
- **Lifecycle/obsolescence audit** — needs MPNs + network; skipped for the
  same reason.
- **Gerber analysis** — no fabrication outputs exist yet for this rev.

## Lower-priority / expected at this stage

`VP-001` (48× untented via-in-pad, warning) and `GP-001` (reference-plane
gaps, mostly on connector/header nets) are numerous but generally expected
for a hand-routed board at this stage — not singled out here individually.
Worth a pass once the CN71 interface is added and the stackup fix above
lands, since re-routing will touch a lot of this anyway.
