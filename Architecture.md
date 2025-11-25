# ☕ Coffee Machine Water Level Sensor — Firmware README

**MCU:** ATtiny202  
**Sensor:** TI FDC1004 (capacitive)  
**Driver:** DRV8210 H-Bridge (for passive piezo)  
**Switch:** TPS22860 (load switch for power gating)  
**Power:** Coin cell (CR2032 / CR2477)  
**Goal:** Ultra-low-power 3-level water sensor with 5-minute audio alerts

---

## 0. Overview

This firmware runs on an **ATtiny202** and monitors water level via an **FDC1004 capacitive sensor** placed _outside_ a plastic tank.  
It measures capacitance differentially between three level electrodes (CIN1–CIN3) and an “always-wet” reference electrode (CIN4).

Every **10 s**:

1. The MCU wakes from deep sleep.
2. It enables the **switched power rail** (`VDD_SW`) powering the FDC1004 and DRV8210.
3. It performs three **single-shot differential measurements** `(CINx – CIN4)`.
4. It determines the level (Normal / Low / Very-Low / Critical).
5. If a threshold is crossed, it schedules a 5-minute alert pattern.
6. It powers down peripherals and returns to sleep.

Alerts (escalating burst density):
| Level | Pattern | Duration | Cadence | Duty Cycle | Avg Current (window) | Weekly Added |
|:--|:--|:--|:--|:--|:--|:--|
| Low | 2 beeps (100 ms each) | 5 min | every 10 s | 2% | ~120 µA | +0.060 µA |
| Very-Low | 3 beeps (100 ms each) | 5 min | every 8 s | 3.75% | ~225 µA | +0.112 µA |
| Critical | 5 beeps (100 ms each) | 5 min | every 5 s | 10% | ~600 µA | +0.298 µA |

Button:

- **Long press (3 s):** calibration ("learn" baseline).
- **Hold 5 s on boot:** factory reset.

---

## 1. Pin Assignments

| Function          | Net Name      | ATtiny202 Pin   | Notes                                                                                           |
| ----------------- | ------------- | --------------- | ----------------------------------------------------------------------------------------------- |
| **UPDI / Button** | `UDPI_BTN`    | **PA0 (pin 6)** | UPDI programming + user button via series 220 Ω; enable internal pull-up; wake source. |
| **PWR_EN**        | `PWR_EN`      | **PA1 (pin 4)** | Controls TPS22860 (active-high). Powers FDC1004 + DRV8210. 100kΩ pull-down to GND (OFF by default). |
| **I²C SDA**       | `SDA`         | **PA6 (pin 2)** | TWI data.                                                                                       |
| **I²C SCL**       | `SCL`         | **PA7 (pin 3)** | TWI clock.                                                                                      |
| **Piezo IN1**     | `DRV_IN1`     | **PA3 (pin 7)** | PWM output from TCA0.                                                                           |
| **LED**           | `LED`         | **PA2 (pin 5)** | Optional status LED.                                                                            |
| **VDD**           | 3 V Coin Cell | pin 1           | Always-on rail for MCU.                                                                         |
| **GND**           | –             | pin 8           | —                                                                                               |

**I²C pull-ups:** 4.7 kΩ → `VDD_SW`
**I²C speed:** 100 kHz (default)
**FDC1004 address:** `0x50` (7-bit)

⚠️ **Critical:** Before entering sleep, TWI peripheral MUST be disabled and PA6/PA7 configured as high-impedance inputs (no pull-ups) to prevent ~1mA leakage current through pull-ups when VDD_SW = 0V. This is essential for meeting the power budget.

---

## 2. Power Domains

- **Always-on (VDD):** MCU (ATtiny202 core) + LED.
- **Switched (`VDD_SW`):** FDC1004 + DRV8210 via TPS22860.
- MCU drives `PWR_EN` HIGH to enable VDD_SW only during measurement or beeping.
- 100kΩ pull-down on PWR_EN ensures VDD_SW is OFF by default at boot.

---

## 3. FDC1004 Configuration

| Parameter          | Setting                    | Notes                    |
| ------------------ | -------------------------- | ------------------------ |
| Address            | 0x50                       | Fixed device ID          |
| Mode               | Differential (CINx – CIN4) | Noise-immune             |
| Rate               | 100 S/s                    | Best per-sample SNR      |
| Trigger            | Single-shot (`REPEAT=0`)   | Minimizes power          |
| CAPDAC             | Disabled                   | Not used in diff mode    |
| Shield             | SHLD1→CHA, SHLD2→CHB       | Follows electrodes       |
| Conversion time    | ≈10 ms                     | at 100 S/s               |
| Conversion current | 0.75–0.95 mA               | Only during 10 ms active |
| Range              | ±15 pF                     | differential             |
| ΔC swing           | ~0.1–3 pF (typ)            | depends on tank geometry |

**MEAS mapping:**

- MEAS1 = CIN1 – CIN4 → Low
- MEAS2 = CIN2 – CIN4 → Very-Low
- MEAS3 = CIN3 – CIN4 → Critical

---

## 4. Piezo / H-Bridge Drive

**DRV8210DSGR Configuration:**
- **Variant:** DSGR package (no nSLEEP pin - power controlled via VDD_SW load switch)
- **MODE pin:** Tied HIGH (enables single-input complementary mode)
- **IN1:** Driven by PA3 PWM (TCA0 WO0)
- **IN2:** Pulled to VDD_SW via 100kΩ (logic enable)
- **Operation:** In MODE=HIGH, DRV8210 internally generates complementary outputs from single PWM input on IN1, creating true AC drive for piezo

| Parameter      | Value                                                  | Notes                                  |
| -------------- | ------------------------------------------------------ | -------------------------------------- |
| Tone frequency | ≈2.7 kHz                                               | Adjust for resonance (2–4 kHz typical) |
| Duty cycle     | 50 %                                                   | Square wave                            |
| PWM source     | TCA0 WO0 (single-slope) on PA3                         |
| Beep duration  | 100 ms on / 100 ms gap between beeps                   |
| Patterns       | Low = 2 beeps; Very-Low = 3 beeps; Critical = 5 beeps |
| Cadence        | Low: every 10s; Very-Low: every 8s; Critical: every 5s |
| Power gating   | Enable `PWR_EN` only during burst sequences            |

---

## 5. Thresholds & Calibration

| Setting    | Default | Units | Purpose            |
| ---------- | ------- | ----- | ------------------ |
| ΔC_low     | 800     | fF    | Low threshold      |
| ΔC_vlow    | 500     | fF    | Very-Low threshold |
| ΔC_crit    | 300     | fF    | Critical threshold |
| Hysteresis | 10 %    | —     | Prevents chatter   |

**Calibration (learn mode)**
Long press (3 s) with tank full → samples 16 × per channel → stores baseline ΔC offsets in EEPROM.
Used to auto-zero the readings (still differential vs CIN4).

**Calibration Safeguards:**
- **Minimum value check:** Each CINx-CIN4 reading must be > 200 fF to prevent empty-tank calibration
- **Range check:** Baseline values must be within ±5 pF to be considered valid
- **CIN4 validation:** Reference electrode must show consistent readings (σ < 100 fF across samples)
- **Failure handling:** If validation fails, LED blinks error code and calibration is rejected
- **Factory defaults:** If EEPROM corrupted or invalid, use hardcoded safe defaults from table above

---

## 6. Alert Timing & Behavior

| Level    | Pattern  | Repeat Rate | Window Length | Active Time/Period | Behavior                       |
| -------- | -------- | ----------- | ------------- | ------------------ | ------------------------------ |
| Low      | 2 beeps  | Every 10 s  | 5 min         | 200 ms / 10 s      | Beep → silence after window    |
| Very-Low | 3 beeps  | Every 8 s   | 5 min         | 300 ms / 8 s       | Escalates if Critical reached  |
| Critical | 5 beeps  | Every 5 s   | 5 min         | 500 ms / 5 s       | Highest priority, hard to ignore |
| Refill   | —        | —           | —             | —                  | Silent until level drops again |

**Beep Structure:** Each beep is 100 ms tone + 100 ms gap between beeps. Power gated per burst sequence.

**Behavior:**
- **Escalation:** Restart 5-min timer at new higher level.
- **De-escalation:** Stay silent when refilled.

---

## 7. Button & LED Functions

**Button (PA0)**
| Action | Duration | Function |
|---------|-----------|-----------|
| Long press | ≥3 s | Learn calibration |
| Hold on boot | ≥5 s | Factory reset |

**LED (PA2)**
| Event | Pattern |
|-------|----------|
| Boot | 1 quick flash |
| Calibration mode | slow blink; 3× fast blink on success |
| Calibration failed | 5× fast blink (invalid baseline values) |
| Fault I²C timeout | 2 blinks |
| CIN4 error | 3 blinks |

---

## 8. Error Handling

- **CIN4 validation:** If "always-wet" reference < min_ref for 3 ticks → suppress alerts + blink fault.
- **I²C timeout:** 20 ms limit; retry once; then soft-reset FDC1004 (`RST` bit).
- **Watchdog:** ≈8 s timeout (or disabled during sleep); fed before entering sleep and after wake.
- **Fail-safe:** Sensor fail = silent; no false beeps.
- **Calibration validation:** Reject calibration if values are out of range or inconsistent (see §5).

---

## 9. Power Budget

**Baseline (no alerts):**
| Mode               | Active current    | Duration | Average |
| ------------------ | ----------------- | -------- | ------- |
| Sleep (Power-Down) | 0.1–0.7 µA        | 99.6%    | ~0.1 µA |
| Measure (every 10s)| 0.8 mA @ 20–30 ms | 0.3%     | ~2–3 µA |
| **Baseline total** | —                 | —        | **~3.5 µA** |

**Alert overhead (5-min window once per week per level):**
| Level    | Avg Current (window) | Weekly Added Current |
| -------- | -------------------- | -------------------- |
| Low      | ~120 µA              | +0.060 µA            |
| Very-Low | ~225 µA              | +0.112 µA            |
| Critical | ~600 µA              | +0.298 µA            |
| **Total alert overhead** | — | **+0.47 µA** |

**Overall average current:** ~3.5 µA baseline + ~0.47 µA alerts = **~3.97 µA**

**Battery life estimates:**
| Cell Type | Capacity | Estimated Life | Practical Life* |
| --------- | -------- | -------------- | --------------- |
| CR2032    | 220 mAh  | ~6.3 years     | 2–3 years       |
| CR2477    | 1000 mAh | ~29 years      | 5+ years        |

*Limited by cell self-discharge (~1%/year) and shelf life

---

## 10. Timers & Clocks

| Function     | Source               | Notes                        |
| ------------ | -------------------- | ---------------------------- |
| System clock | 20 MHz RC            | div / prescale for low-power |
| 10 s wake    | RTC/PIT @ 32 kHz ULP | ± drift OK                   |
| PWM tone     | TCA0 (2.7 kHz)       | WO0 on PA3                   |
| Button wake  | Pin-change INT PA0   | Pull-up enabled              |

---

## 11. EEPROM Layout (≤64 B)

```c
struct __attribute__((packed)) NvmConfig {
  uint16_t version;           // 0x0001
  uint16_t th_low_ff;
  uint16_t th_vlow_ff;
  uint16_t th_crit_ff;
  uint16_t hysteresis_pct;
  int16_t  base_c1_ff;
  int16_t  base_c2_ff;
  int16_t  base_c3_ff;
  uint8_t  install_ok;
  uint8_t  reserved[5];
  uint16_t crc16;
};
```

## 12. Software Architecture

/firmware
├─ src/
│ ├─ pins.hpp // Pin definitions
│ ├─ power.cpp/.h // Sleep / PWR_EN / RTC / WDT
│ ├─ twi.cpp/.h // Minimal I²C driver (100 kHz)
│ ├─ fdc1004.cpp/.h // Register map + single-shot
│ ├─ level_logic.cpp/.h // ΔC calc + hysteresis + debounce
│ ├─ alert_manager.cpp/.h // 5-min windows + scheduling
│ ├─ buzzer.cpp/.h // PWM tone + H-bridge envelopes
│ ├─ button.cpp/.h // Press detection (shared UPDI)
│ ├─ eeprom.cpp/.h // CRC + persistence
│ └─ main.cpp // State machine glue
└─ README.md

## 13. Build & Toolchain

| Item            | Recommendation                                    |
| --------------- | ------------------------------------------------- |
| Toolchain       | **avr-gcc** or PlatformIO (tinyAVR 0/1 framework) |
| IDE             | VS Code + PlatformIO                              |
| Programmer      | UPDI adapter (1528-5879-ND) via TC2030-IDC-NL     |
| Debug           | LED codes + piezo tones (no UART)                 |
| Optimization    | `-Os` + LTO enabled                               |
| Expected binary | < 1 KB Flash / < 64 B RAM                         |

## 14. Test & Validation Plan

Bring-up

Verify PWR_EN toggles and VDD_SW rise (~3 ms).

Check I²C comms @100 kHz; read FDC1004 ID registers.

Record ΔC (air → water) for each channel.

Alert Patterns

Force Low/Very-Low/Critical states; verify double/triple/continuous beeps, 5 min window, button silence, escalation.

Power Consumption

Measure avg current idle/measure/beep; validate budget.

Calibration & EEPROM

Long press (3 s): learn; check LED confirmation and EEPROM update.

Fault Cases

Disconnect FDC1004 → I²C timeout; verify no false beeps.

Dry CIN4 → install_error; verify fault blink code

## 15. Installation Validation

After installation, the system will automatically validate sensor placement on first boot:

- Boot LED flash → system starting
- If CIN4 reading is valid (reference electrode properly positioned) → normal operation begins
- If CIN4 validation fails → 3 LED blinks (CIN4 error) → re-position sensor needed

After stable installation, perform calibration with full tank (long press 3 s).
