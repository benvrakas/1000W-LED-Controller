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

## Error Code Patterns

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

| Step | Pattern | Visual Pattern | Boot Step Description |
|------|---------|----------------|-----------------------|
| 1 | `0` | Short | **Board Pins**: GPIO configuration or safety check failed. |
| 2 | `00` | Short - Short | **Pump**: Pump initialization or priming failed. |
| 3 | `000` | Short - Short - Short | **Fans**: Fan controller initialization failed. |
| 4 | `1` | Long | **PSU**: CAN communication or PSU response timeout. |
| 5 | `11` | Long - Long | **Display**: OLED initialization failed. |

---
*Note: If multiple errors are active, the LED will cycle through all active patterns with an inter-code pause between them.*
