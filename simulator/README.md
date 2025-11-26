# PC Simulator - No Hardware Needed

## Overview

This PC-based simulator runs the complete firmware logic without any hardware. Perfect for:
- ✅ Validating firmware behavior before hardware arrives
- ✅ Testing level detection logic
- ✅ Verifying beep patterns
- ✅ Power budget calculations
- ✅ Demonstrating system to stakeholders

---

## Quick Start

### Windows (with g++)

```powershell
# Compile
g++ simulator/simulator.cpp -o simulator.exe

# Run (100x accelerated)
./simulator.exe
```

### Linux/Mac

```bash
# Compile
g++ simulator/simulator.cpp -o simulator

# Run (100x accelerated)
./simulator
```

### Alternative: WSL (Windows Subsystem for Linux)

```bash
# In WSL terminal
cd /mnt/c/Users/JeremySnudden/Documents/Personal/CoffeeWaterLevelSensor/LevelSensorV0.2/Firmware/levelUP
g++ simulator/simulator.cpp -o simulator
./simulator
```

---

## What You'll See

The simulator displays real-time status in your terminal:

```
╔═══════════════════════════════════════════════════════════════╗
║     ATtiny202 Water Level Sensor - PC Simulator              ║
╚═══════════════════════════════════════════════════════════════╝

┌───────────────────────────────────────────────────────────────┐
│ Runtime: 00:05:30   Cycles: 33   Avg Current: 4.52 µA        │
└───────────────────────────────────────────────────────────────┘

━━━━━━━━━━━━━━━━━━ CYCLE 33 ━━━━━━━━━━━━━━━━━━

⏰ WAKE EVENT (every 10 seconds)
  └─ RTC/PIT interrupt triggered

🔌 POWER MANAGEMENT
  └─ PWR_EN = HIGH (VDD_SW enabled)
  └─ FDC1004 + DRV8210 powered on

📡 SENSOR READING
  └─ Test Level: VERY-LOW (Below threshold 2)
  └─ CIN1 - CIN4: 600 fF
  └─ CIN2 - CIN4: 400 fF
  └─ CIN3 - CIN4: 1000 fF

💧 WATER LEVEL
  └─ Detected: █ VERY-LOW █
  └─ Previous: LOW

⚠️  LEVEL CHANGE DETECTED!
  └─ LOW → VERY-LOW

  🔊 BEEP: ♪ ♪ ♪ (3 beeps)

🔌 POWER MANAGEMENT
  └─ PWR_EN = LOW (VDD_SW disabled)
  └─ TWI disabled, I2C pins high-Z

💤 SLEEP MODE
  └─ STANDBY mode (RTC running)
  └─ Sleep current: 0.50 µA
  └─ Sleeping for 10 seconds...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Simulation Behavior

### Level Cycling (Same as Hardware Test Bench)

| Time | Level | Beeps | Simulated Readings |
|------|-------|-------|-------------------|
| 0-30s | NORMAL | None | C1=1200, C2=1100, C3=1000 fF |
| 30-60s | LOW | 2 beeps | C1=600, C2=1100, C3=1000 fF |
| 60-90s | VERY-LOW | 3 beeps | C1=600, C2=400, C3=1000 fF |
| 90-120s | CRITICAL | 5 beeps | C1=600, C2=400, C3=200 fF |
| 120s+ | *Cycle repeats* |

### Display Elements

**Status Bar:**
- Runtime: Simulated elapsed time
- Cycles: Number of 10-second wake cycles
- Avg Current: Calculated average power consumption

**Color Coding:**
- 🟢 Green: NORMAL level
- 🟡 Yellow: LOW level
- 🟠 Orange: VERY-LOW level
- 🔴 Red: CRITICAL level

**Battery Estimate (every 100 cycles):**
```
╔═══════════════════════════════════════════════════════════════╗
║                    BATTERY LIFE ESTIMATE                      ║
╚═══════════════════════════════════════════════════════════════╝

Average Current: 4.52 µA

CR2032 (220 mAh):
  └─ Estimated life: 5.6 years
  └─ Practical life: ~2 years (50% derating)

CR2477 (1000 mAh):
  └─ Estimated life: 25.2 years
  └─ Practical life: ~12 years (50% derating)
```

---

## Configuration

Edit `simulator.cpp` to change behavior:

### Real-Time vs Accelerated

```cpp
constexpr bool REAL_TIME = false;  // false = accelerated
constexpr int TIME_SCALE = 100;    // 100x faster than real hardware
```

**Real-time mode:** 10-second cycles (like actual hardware)
**Accelerated mode:** 100ms cycles (default, 100x faster)

### Custom Test Levels

```cpp
const TestLevel TEST_LEVELS[] = {
    {1200, 1100, 1000, "NORMAL (Full tank)"},
    {600, 1100, 1000, "LOW"},
    {600, 400, 1000, "VERY-LOW"},
    {600, 400, 200, "CRITICAL"}
};
```

### Adjust Thresholds

```cpp
constexpr int16_t THRESHOLD_LOW_FF = 800;   // Change threshold
constexpr int16_t THRESHOLD_VLOW_FF = 500;
constexpr int16_t THRESHOLD_CRIT_FF = 300;
```

### Power Consumption Values

```cpp
constexpr double CURRENT_SLEEP = 0.5;      // Sleep mode (µA)
constexpr double CURRENT_WAKE = 800.0;     // Active (µA)
constexpr double CURRENT_BEEP = 50000.0;   // Beeping (µA = 50mA)
```

---

## Validation Checklist

Use this checklist to verify firmware logic:

### ✅ Level Detection
- [ ] Normal → Low transition at C1 < 800 fF
- [ ] Low → Very-Low transition at C2 < 500 fF
- [ ] Very-Low → Critical transition at C3 < 300 fF
- [ ] All transitions trigger correct beeps

### ✅ Beep Patterns
- [ ] LOW: 2 beeps
- [ ] VERY-LOW: 3 beeps
- [ ] CRITICAL: 5 beeps
- [ ] No beeps when returning to NORMAL

### ✅ Power Management
- [ ] PWR_EN toggles correctly
- [ ] Sleep current: ~0.5 µA
- [ ] Wake current: ~800 µA (0.8 mA)
- [ ] Beep current: ~50 mA

### ✅ Timing
- [ ] 10-second wake cycles
- [ ] 30 seconds per level (3 cycles)
- [ ] 120 seconds per full cycle (4 levels)

### ✅ Power Budget
- [ ] Average current: 3-5 µA (no alerts)
- [ ] CR2032 life: 2-3 years practical
- [ ] CR2477 life: 5+ years practical

---

## Comparison: Simulator vs Hardware

| Feature | PC Simulator | Hardware Test | Real Deployment |
|---------|-------------|---------------|-----------------|
| **Speed** | 100x faster | Real-time | Real-time |
| **Sensor** | Simulated | Simulated | Real FDC1004 |
| **Power** | Calculated | Measured | Measured |
| **Beeps** | Text output | Audio | Audio |
| **Setup** | Just compile | Minimal HW | Full system |
| **Cost** | Free | ~$5 | ~$15 |

---

## Troubleshooting

### Issue: Compiler not found

**Windows:**
Install MinGW-w64 or use WSL

**Mac:**
```bash
xcode-select --install
```

**Linux:**
```bash
sudo apt install g++
```

### Issue: Won't compile

Make sure you're using C++11 or later:
```bash
g++ -std=c++11 simulator/simulator.cpp -o simulator
```

### Issue: Terminal colors don't work (Windows)

Use Windows Terminal or WSL for ANSI color support

---

## Next Steps

1. **✅ Run Simulator** - Validate firmware logic
2. **✅ Adjust Thresholds** - If needed for your tank
3. **Build for Hardware**:
   ```bash
   pio run -e attiny202_test    # Hardware test bench
   pio run -e attiny202         # Real sensor version
   ```
4. **Test on Real Hardware** - When available
5. **Upgrade to ATtiny402** - Full features

---

## Files in This Directory

```
simulator/
├── simulator.cpp    # PC simulator source
└── README.md        # This file
```

---

## Example Session

```bash
$ g++ simulator/simulator.cpp -o simulator
$ ./simulator

ATtiny202 Water Level Sensor - PC Simulator

Simulation Mode: ACCELERATED
Time Scale: 100x faster

Starting simulation in 3 seconds...

[Cycles through all levels automatically]
[Shows power consumption in real-time]
[Calculates battery life every 100 cycles]

^C  # Press Ctrl+C to stop

Simulation stopped.

╔═══════════════════════════════════════════════════════════════╗
║                    BATTERY LIFE ESTIMATE                      ║
╚═══════════════════════════════════════════════════════════════╝

Average Current: 4.23 µA

CR2032 (220 mAh):
  └─ Estimated life: 5.9 years
  └─ Practical life: ~2 years (50% derating)
```
