# NeoPixel Error Codes

The onboard NeoPixel on the Feather M4 CAN Express is used as a fallback status indicator. In case of a system error, the LED will blink a binary pattern in **RED**.

## Pulse Definitions
- **Short Pulse**: 300ms (Binary `0`)
- **Long Pulse**: 600ms (Binary `1`)
- **Inter-bit Pause**: 150ms (LED OFF)
- **Inter-code Pause**: 1000ms (LED OFF)

## Status Colors
- **BLUE**: System Initializing (`INIT`)
- **GREEN**: System Running (`RUN`)
- **RED (Blinking)**: System Error (`ERROR_KILL`)
- **AMBER (Solid)**: One or more fault channels ignored. Holding the power
  button for 3s while latched in `ERROR_KILL` does not disable fault
  checking globally -- it identifies and permanently ignores (until a
  physical power cycle) only the specific channel that was actively at
  fault (LED temp, water temp, pump, main fan, PSU fan, aux fan, PSU comms,
  or encoder), then returns to normal operation. Every other channel stays
  fully monitored; a fault on a *different*, non-ignored channel still
  latches `ERROR_KILL` normally. Solid, not blinking, since blinking is
  reserved for an active latched error. The diagnostics screen (scroll with
  the knob) shows `IGNORED` on any channel's page instead of a pass/fail
  verdict, and the RUN screen's status line shows `[N IGNORED]`.

## Error Code Patterns

All seven fault codes below are individually ignorable via the 3s button
hold described under [AMBER](#status-colors) above — the hold identifies and
ignores only the channel that's actually at fault.

| Fault Code | Pattern (Binary) | Visual Pattern | Description |
|------------|------------------|----------------|-------------|
| 1 | `1` | Long | **INIT_FAILED**: Boot sequence timeout or hardware init failure. |
| 2 | `10` | Long - Short | **CAN_TIMEOUT**: Lost communication with the PSU. |
| 3 | `11` | Long - Long | **PSU_FAULT**: PSU internal error reported. |
| 4 | `010` | Short - Long - Short | **OVER_TEMP_LED**: LED temperature exceeded safety limits. |
| 5 | `011` | Short - Long - Long | **OVER_TEMP_WATER**: Water/Coolant temperature exceeded safety limits. |
| 6 | `00` | Short - Short | **COOLING_FAILURE**: Fan or pump stall/failure detected. |
| 7 | `001` | Short - Short - Long | **ENCODER_FAULT**: Invalid signal or failure from UI encoder. |

## Initialization Failure Codes (BLUE)
If the system fails during the boot sequence, the NeoPixel will blink **BLUE**. The pattern corresponds to the failed boot step.

| Step | ID | Pattern | Visual Pattern | Boot Step Description |
|------|----|---------|----------------|-----------------------|
| 1 | 101 | `0` | Short | **Board Pins**: GPIO configuration or safety check failed. |
| 2 | 102 | `00` | Short - Short | **Pump**: Pump did not reach `PUMP_STALL_RPM` within the boot step's `BOOT_STEP_TIMEOUT_MS` (2s) budget. The pump is held at `PUMP_SPINUP_DUTY` (~50%) for `SPINUP_MS` before its tach is judged, so this means no/low tach feedback — a dead pump, a disconnected tach line, or a dry loop. Ignorable via the 3s hold in `ERROR_KILL` (see [AMBER](#status-colors) above) once latched. |
| 3 | 103 | `000` | Short - Short - Short | **Fans**: One of the radiator / PSU / aux fans did not reach its stall threshold within the boot step's `BOOT_STEP_TIMEOUT_MS` (2s) budget. Same shape as step 2: all three are held at ~50% spin-up duty for `SPINUP_MS` first. The PSU fan's tach is currently untrusted and its stall check disabled entirely (`TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED = false`), so this can only mean the radiator or aux fan. Check the serial log — it names nothing, so build with `-D COOLING_BOOT_TRACE` (see `platformio.ini`) to see per-channel duty/RPM. Ignorable via the 3s hold in `ERROR_KILL` once latched. |
| 4 | 104 | `1` | Long | **PSU**: CAN communication or PSU response timeout. |
| 5 | 105 | `11` | Long - Long | **Display**: OLED initialization failed. |

---
*Note: If multiple errors are active, the LED will cycle through all active patterns with an inter-code pause between them.*
