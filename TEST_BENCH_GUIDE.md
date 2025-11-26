# Test Bench Guide

## Overview

This guide covers complete firmware testing without needing a water tank or FDC1004 sensor. The test firmware simulates sensor readings and cycles through all water levels automatically.

---

## Test Firmware Build

### Build Test Version
```bash
pio run -e attiny202_test
```

### Upload to Hardware
```bash
pio run -e attiny202_test -t upload
```

**Flash Used: ~1100 bytes (no I2C or FDC1004 driver needed)**

---

## Test Firmware Behavior

### Automatic Level Cycling

The test firmware automatically cycles through these levels every 30 seconds:

| Time | Level | LED Blinks | Beep Pattern | Simulated Readings (fF) |
|------|-------|-----------|--------------|------------------------|
| 0-30s | NORMAL | 1 blink | None | C1=1200, C2=1100, C3=1000 |
| 30-60s | LOW | 2 blinks | 2 beeps | C1=600, C2=1100, C3=1000 |
| 60-90s | VERY-LOW | 3 blinks | 3 beeps | C1=600, C2=400, C3=1000 |
| 90-120s | CRITICAL | 4 blinks | 5 beeps | C1=600, C2=400, C3=200 |
| 120s | *Long blink* | Test cycle complete - repeat |

### LED Indicators

**On every 10-second wake:**
- LED blinks show current test level:
  - 1 blink = Normal
  - 2 blinks = Low
  - 3 blinks = Very-Low
  - 4 blinks = Critical

**On boot:**
- 5 fast blinks = Test mode starting

**After each full cycle (120s):**
- Long 500ms blink = Cycle complete

### Beep Behavior

Beeps occur when transitioning FROM Normal TO a problem level:
- Normal → Low: **2 beeps**
- Low → Very-Low: **3 beeps**
- Very-Low → Critical: **5 beeps**
- Critical → Normal: **No beeps** (problem resolved)

---

## Hardware Test Bench Setup

### Minimum Setup (Software Testing)

**Required:**
- ATtiny202 on PCB or breadboard
- 3V power supply (coin cell or bench supply)
- UPDI programmer connected

**Optional (recommended):**
- LED on PA2 (with 220Ω resistor to GND)
- Piezo buzzer + DRV8210 for beep testing
- TPS22860 for power gating testing

### Full Setup (Complete System Test)

**All components:**
1. **ATtiny202** - Main MCU
2. **TPS22860** - Load switch (PWR_EN control)
3. **DRV8210** - H-bridge for piezo
4. **Piezo buzzer** (CPT-9019A)
5. **LED** on PA2
6. **Power supply** - 3V coin cell or bench supply
7. **Current meter** - To measure sleep/active current
8. **UPDI programmer** - For flashing

### Wiring Diagram

```
ATtiny202 Pin Connections:
┌─────────────────────────┐
│ PA0 (pin 6) - UPDI      │ ← UPDI programmer
│ PA1 (pin 4) - PWR_EN    │ → TPS22860 ON pin
│ PA2 (pin 5) - LED       │ → LED + 220Ω → GND
│ PA3 (pin 7) - DRV_IN1   │ → DRV8210 IN1
│ PA6 (pin 2) - SDA       │ (not used in test)
│ PA7 (pin 3) - SCL       │ (not used in test)
│ VDD (pin 1) - 3V        │ ← Power supply +
│ GND (pin 8) - GND       │ ← Power supply -
└─────────────────────────┘

TPS22860 (Load Switch):
- VIN: 3V
- ON: PA1 (PWR_EN)
- VOUT: VDD_SW → DRV8210 VCC
- GND: GND

DRV8210 (H-Bridge):
- VCC: VDD_SW (from TPS22860)
- IN1: PA3 (PWM from MCU)
- IN2: VDD_SW (pulled high via 100kΩ)
- MODE: VDD_SW (pulled high - complementary mode)
- OUT1/OUT2: Piezo buzzer
- GND: GND
```

---

## Test Procedures

### Test 1: Boot Sequence
**Expected Behavior:**
1. Power on
2. LED blinks 5 times rapidly (test mode indicator)
3. LED shows 1 blink (Normal level)

**✅ Pass Criteria:**
- 5 fast blinks on boot
- 1 blink after ~2 seconds

---

### Test 2: Level Cycling
**Expected Behavior:**
Every 10 seconds, LED blinks to show level:
- Cycle 1-3 (0-30s): 1 blink (Normal)
- Cycle 4-6 (30-60s): 2 blinks (Low)
- Cycle 7-9 (60-90s): 3 blinks (Very-Low)
- Cycle 10-12 (90-120s): 4 blinks (Critical)
- Long blink, then repeat

**✅ Pass Criteria:**
- LED blink count increases correctly
- Cycle repeats after ~120 seconds

---

### Test 3: Beep Patterns
**Expected Behavior:**
- At 30s (Normal→Low): **2 beeps** (100ms each, 100ms gap)
- At 60s (Low→Very-Low): **3 beeps**
- At 90s (Very-Low→Critical): **5 beeps**
- At 120s (Critical→Normal): **No beeps**

**✅ Pass Criteria:**
- Correct number of beeps
- Clear tone (~2.7 kHz)
- Correct timing (100ms on, 100ms off)

**🔧 Troubleshooting:**
- No sound? Check DRV8210 connections
- Wrong frequency? Adjust TCA0.SINGLE.PER in buzzer.cpp

---

### Test 4: Power Gating (VDD_SW Control)
**Setup:**
- Oscilloscope on PWR_EN pin (PA1)
- Optional: Probe VDD_SW output from TPS22860

**Expected Behavior:**
- PWR_EN goes HIGH at start of each 10s cycle
- PWR_EN stays HIGH during beep
- PWR_EN goes LOW after beep completes
- PWR_EN is LOW during sleep

**✅ Pass Criteria:**
- PWR_EN pulses match wake cycles
- VDD_SW follows PWR_EN with ~3ms rise time

---

### Test 5: Sleep Current
**Setup:**
- Current meter in series with power supply
- Measure during sleep period (between LED blinks)

**Expected Behavior:**
- Sleep current: < 1 µA typical
- Wake current: ~1-2 mA for ~50ms

**✅ Pass Criteria:**
- Average current over 10s cycle: < 10 µA
- Sleep current: < 2 µA

**🔧 Troubleshooting:**
- High sleep current (>10µA)?
  - Check TWI disabled (PA6/PA7 high-Z)
  - Verify PWR_EN is LOW during sleep
  - Check for floating inputs

---

### Test 6: RTC Wake Timing
**Setup:**
- Oscilloscope on LED pin (PA2)
- Trigger on rising edge

**Expected Behavior:**
- LED blinks every 10.0 seconds ± 1%
- Timing consistent across multiple cycles

**✅ Pass Criteria:**
- Period: 9.9 - 10.1 seconds
- Jitter: < 100ms

---

### Test 7: Full Cycle Endurance
**Duration:** 30 minutes (15 complete cycles)

**Expected Behavior:**
- Continuous operation
- No crashes or hangs
- Consistent timing
- No pattern errors

**✅ Pass Criteria:**
- All 15 cycles complete successfully
- LED and beep patterns remain correct
- No resets or unexpected behavior

---

## Measurement Points

### Current Measurement
```
[Power Supply +] → [Current Meter] → [VDD] → ATtiny202
```

### Oscilloscope Probes
- **Ch1**: PA1 (PWR_EN) - Power gating
- **Ch2**: PA2 (LED) - Cycle timing
- **Ch3**: PA3 (DRV_IN1) - PWM buzzer signal
- **Ch4**: VDD_SW - Load switch output

---

## Test Results Template

```
┌─────────────────────────────────────────────────┐
│  ATtiny202 Water Level Sensor - Test Results   │
└─────────────────────────────────────────────────┘

Date: __________
Firmware Version: attiny202_test
Board: __________

BOOT SEQUENCE:
[ ] 5 fast blinks observed
[ ] Initial level = Normal (1 blink)

LEVEL CYCLING:
[ ] Normal (1 blink) - 30s
[ ] Low (2 blinks) - 30s
[ ] Very-Low (3 blinks) - 30s
[ ] Critical (4 blinks) - 30s
[ ] Cycle repeats correctly

BEEP PATTERNS:
[ ] Low: 2 beeps at 30s
[ ] Very-Low: 3 beeps at 60s
[ ] Critical: 5 beeps at 90s
[ ] Tone frequency: ~2.7 kHz
[ ] Beep timing: 100ms on / 100ms off

POWER GATING:
[ ] PWR_EN toggles correctly
[ ] VDD_SW follows PWR_EN
[ ] Rise time: ~3ms

SLEEP CURRENT:
Sleep: ______ µA (target: <2 µA)
Wake: ______ mA (target: 1-2 mA)
Average: ______ µA (target: <10 µA)

RTC TIMING:
Period: ______ seconds (target: 10.0s)
Jitter: ______ ms (target: <100ms)

ENDURANCE (30 min):
[ ] No crashes
[ ] Consistent timing
[ ] No pattern errors

OVERALL RESULT: [ ] PASS  [ ] FAIL

Notes:
_________________________________________________
_________________________________________________
```

---

## Common Issues & Solutions

### Issue: No LED Activity
**Causes:**
- LED not connected or wrong polarity
- LED pin (PA2) not configured
- MCU not running

**Solutions:**
- Check LED + resistor to GND on PA2
- Verify power supply voltage (2.7-3.6V)
- Check UPDI connection

---

### Issue: No Beeps
**Causes:**
- DRV8210 not powered (VDD_SW = 0V)
- PWM not reaching DRV8210
- Piezo not connected

**Solutions:**
- Verify PWR_EN goes HIGH
- Check TPS22860 VOUT = VDD_SW
- Probe PA3 for PWM signal (~2.7 kHz)
- Check DRV8210 IN1 connection

---

### Issue: Wrong Beep Count
**Causes:**
- Threshold values incorrect
- Level logic error

**Solutions:**
- Check TEST_LEVELS array in main_test.cpp
- Verify threshold defines match expected behavior

---

### Issue: High Sleep Current (>10µA)
**Causes:**
- TWI not disabled (leakage through I2C pull-ups)
- PWR_EN not going LOW
- Floating inputs

**Solutions:**
- Check power_disable_peripherals() called
- Verify PA6/PA7 configured as high-Z inputs
- Check PWR_EN = LOW during sleep
- Ensure no floating pins

---

## Next Steps After Test Bench

Once all tests pass:

1. **✅ Test Bench Complete** - Firmware validated
2. **Flash Real Firmware**:
   ```bash
   pio run -e attiny202 -t upload
   ```
3. **Add FDC1004 + Tank** - Real sensor testing
4. **Calibrate Thresholds** - Adjust based on actual readings
5. **Power Budget Validation** - Long-term current measurement
6. **Upgrade to ATtiny402** - Full feature set when ready

---

## Switching Between Versions

### Test Bench → Minimal
```bash
pio run -e attiny202 -t upload
```

### Minimal → Test Bench
```bash
pio run -e attiny202_test -t upload
```

### When ATtiny402 Arrives → Full
```bash
pio run -e attiny402 -t upload
```
