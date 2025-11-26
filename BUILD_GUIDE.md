# Build Guide - ATtiny202 vs ATtiny402

## Overview

Two build configurations are provided:
- **attiny202**: Minimal version for testing (2KB flash)
- **attiny402**: Full-featured version for production (4KB flash)

## Build Commands

### ATtiny202 (Minimal - Testing)
```bash
pio run -e attiny202
```
**Flash Used: 1776 / 2048 bytes (86.7%)**

### ATtiny402 (Full - Production)
```bash
pio run -e attiny402
```

---

## Feature Comparison

### ✅ ATtiny202 Minimal (1776 bytes)

**Included:**
- FDC1004 capacitive sensor reading (CIN1-3 vs CIN4)
- Simple 3-level water detection (Low/Very-Low/Critical)
- Beep patterns: 2 beeps (Low), 3 beeps (Very-Low), 5 beeps (Critical)
- 10-second RTC wake cycle
- Power gating via TPS22860 (VDD_SW control)
- Basic I2C driver (100 kHz)
- PWM buzzer control via DRV8210
- Sleep mode (STANDBY)
- Boot LED blink

**Hardcoded Values:**
- Low threshold: 800 fF
- Very-Low threshold: 500 fF
- Critical threshold: 300 fF

**Removed (to fit in 2KB):**
- ❌ EEPROM config storage
- ❌ CRC16 validation
- ❌ 16-sample calibration mode
- ❌ 5-minute alert windows
- ❌ Hysteresis
- ❌ 3-sample debouncing
- ❌ Button support (long press, factory reset)
- ❌ LED diagnostic codes
- ❌ Alert escalation/de-escalation

**Behavior:**
- Wakes every 10 seconds
- Measures water level
- If level changed from last reading AND is not Normal, beeps immediately
- Returns to sleep

---

### ✅ ATtiny402 Full-Featured (~4100 bytes)

**Includes Everything from Minimal PLUS:**
- ✅ EEPROM config with CRC16 validation
- ✅ 16-sample calibration (long button press 3s)
- ✅ 5-minute alert windows with auto-stop
- ✅ 10% hysteresis on thresholds
- ✅ 3-sample debouncing
- ✅ Button support:
  - Short press: Silence alert
  - Long press (3s): Calibration mode
  - Boot hold (5s): Factory reset
- ✅ LED diagnostic codes:
  - 1 blink: Boot
  - 2 blinks: I2C timeout
  - 3 blinks: CIN4 error (installation issue)
  - 5 blinks: Calibration failed
  - 10 blinks: Factory reset complete
- ✅ Alert escalation (restart timer if level worsens)
- ✅ Alert de-escalation (stop if refilled)
- ✅ Configurable thresholds via EEPROM

**Alert Windows (Full Version Only):**
- Low: 2 beeps every 10s for 5 minutes
- Very-Low: 3 beeps every 8s for 5 minutes
- Critical: 5 beeps every 5s for 5 minutes

**Power Budget:**
- Baseline: ~3.5 µA
- With weekly alerts: ~4 µA average
- CR2032: 2-3 years practical
- CR2477: 5+ years practical

---

## Hardware Requirements

Both configurations work with same hardware:
- ATtiny202 or ATtiny402 (pin-compatible)
- FDC1004 capacitive sensor
- DRV8210 H-bridge
- TPS22860 load switch
- CPT-9019A piezo buzzer
- 3V coin cell (CR2032 or CR2477)

---

## Programming

Both use UPDI programming:
```bash
# Upload to ATtiny202
pio run -e attiny202 -t upload

# Upload to ATtiny402
pio run -e attiny402 -t upload
```

**Programmer:** UPDI adapter (1528-5879-ND) via TC2030-IDC-NL

---

## Testing Workflow

1. **Test on ATtiny202** (current hardware)
   - Basic sensor reading validation
   - Verify I2C communication
   - Check beep patterns
   - Confirm power gating
   - Validate 10s wake cycle

2. **Upgrade to ATtiny402** (future hardware)
   - Flash full-featured firmware
   - Test calibration mode
   - Verify EEPROM persistence
   - Test alert windows
   - Validate button functions

---

## Migration Path

When you receive ATtiny402 chips:

1. Replace ATtiny202 with ATtiny402 on PCB (same pinout)
2. Build full version: `pio run -e attiny402`
3. Upload: `pio run -e attiny402 -t upload`
4. Run calibration (long press button 3s with full tank)
5. Test alert windows

**No hardware changes needed - pin-compatible upgrade!**
