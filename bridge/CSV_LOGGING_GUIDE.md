# CSV Data Logging Guide

This guide explains how to log FDC1004 sensor data to CSV files using the ESP32 bridge and Python logger.

## System Overview

```
ATtiny1614 → [UART] → ESP32 Bridge → [USB] → Python Script → CSV File
(Raw format)         (Parse to CSV)          (Add timestamps)
```

## Quick Start

### 1. Flash ESP32 Bridge (One-time setup)

```bash
cd bridge
pio run -t upload
```

Or use Arduino IDE to upload `esp_uart_bridge.ino`

### 2. Install Python Requirements

```bash
pip install pyserial
```

### 3. Run the Logger

**Auto-detect ESP32:**
```bash
python csv_logger.py -a sensor_data.csv
```

**Specify port manually:**
```bash
# Windows
python csv_logger.py COM3 sensor_data.csv

# Linux/Mac
python csv_logger.py /dev/ttyUSB0 sensor_data.csv
```

**List available ports:**
```bash
python csv_logger.py -l
```

## Data Format

### ESP32 CSV Output

The ESP32 bridge converts ATtiny output to CSV:

**Input from ATtiny:**
```
t=0 c1=456 c2=789 c3=321 c4=654
dC: dc1=-50 dc2=-100 dc3=-200
```

**Output from ESP32:**
```csv
timestamp,c1,c2,c3,c4,dc1,dc2,dc3
0,456,789,321,654,-50,-100,-200
```

### Python Logger Output

The Python script adds real-world timestamps:

```csv
real_time,timestamp,c1,c2,c3,c4,dc1,dc2,dc3
2025-03-15 14:23:01.234,0,456,789,321,654,-50,-100,-200
2025-03-15 14:23:09.456,1,457,790,322,655,-48,-99,-198
```

**Columns:**
- `real_time`: Wall-clock timestamp (YYYY-MM-DD HH:MM:SS.mmm)
- `timestamp`: Sensor measurement count (increments every ~8 seconds)
- `c1`, `c2`, `c3`, `c4`: Raw capacitance readings (femtofarads)
- `dc1`, `dc2`, `dc3`: Drift-corrected deltas (femtofarads)

## Usage Examples

### Basic Logging Session

```bash
# Start logging
python csv_logger.py -a data.csv

# Output:
# ✓ Connected to COM3 at 115200 baud
# ✓ Logging to: data.csv
# Starting data capture...
# Press Ctrl+C to stop
#
# Header: real_time,timestamp,c1,c2,c3,c4,dc1,dc2,dc3
# [   1] 2025-03-15 14:23:01.234 | 0,456,789,321,654,-50,-100,-200
# [   2] 2025-03-15 14:23:09.456 | 1,457,790,322,655,-48,-99,-198
# ...

# Press Ctrl+C to stop
# ✓ Stopped logging. Total lines: 42
# ✓ Files closed
```

### Long-term Data Collection

```bash
# Run in background (Linux/Mac)
nohup python csv_logger.py /dev/ttyUSB0 long_term.csv &

# Run in background (Windows PowerShell)
Start-Process python -ArgumentList "csv_logger.py COM3 long_term.csv" -WindowStyle Hidden
```

### Append to Existing File

The logger automatically appends to existing files:

```bash
# Day 1
python csv_logger.py -a day1.csv
# ... collect data, Ctrl+C ...

# Day 2 - appends to same file
python csv_logger.py -a day1.csv
# ... more data added ...
```

## Data Analysis

### Load in Python (Pandas)

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load CSV
df = pd.read_csv('sensor_data.csv')

# Convert timestamp
df['real_time'] = pd.to_datetime(df['real_time'])

# Plot capacitance over time
plt.figure(figsize=(12, 6))
plt.plot(df['real_time'], df['c1'], label='C1 (Low)')
plt.plot(df['real_time'], df['c2'], label='C2 (Very Low)')
plt.plot(df['real_time'], df['c3'], label='C3 (Critical)')
plt.plot(df['real_time'], df['c4'], label='C4 (Reference)')
plt.xlabel('Time')
plt.ylabel('Capacitance (fF)')
plt.legend()
plt.title('Water Level Sensor Data')
plt.show()

# Plot drift-corrected deltas
plt.figure(figsize=(12, 6))
plt.plot(df['real_time'], df['dc1'], label='dC1')
plt.plot(df['real_time'], df['dc2'], label='dC2')
plt.plot(df['real_time'], df['dc3'], label='dC3')
plt.xlabel('Time')
plt.ylabel('Drift-Corrected Capacitance (fF)')
plt.legend()
plt.title('Drift-Corrected Water Level Deltas')
plt.show()
```

### Load in Excel

1. Open Excel
2. Data → From Text/CSV
3. Select your `.csv` file
4. Excel will auto-detect columns
5. Click Load

### Load in Google Sheets

1. File → Import → Upload
2. Select your `.csv` file
3. Import data

## Troubleshooting

### Python script can't find port

```bash
# List available ports
python csv_logger.py -l

# Try each port manually
python csv_logger.py COM3 test.csv
python csv_logger.py COM4 test.csv
```

### No data appearing

1. Check ESP32 is powered and programmed
2. Verify ATtiny is running (should measure every 8 seconds)
3. Check serial monitor directly:
   ```bash
   # Linux/Mac
   screen /dev/ttyUSB0 115200

   # Windows (Arduino IDE)
   Tools → Serial Monitor → 115200 baud
   ```

### Garbled characters in CSV

- Baud rate mismatch - verify:
  - ATtiny: `LOG_BAUD 9600` in `log_updi_tx.h`
  - ESP32: `UART_BAUD 9600` in `esp_uart_bridge.ino`
  - Python: `--baud 115200` (USB side, not UART)

### CSV format incorrect

- Reflash ESP32 with updated `esp_uart_bridge.ino`
- Verify ESP32 shows header on startup:
  ```
  # ATtiny UPDI Logger Bridge - CSV Mode
  # Format: timestamp,c1,c2,c3,c4,dc1,dc2,dc3
  timestamp,c1,c2,c3,c4,dc1,dc2,dc3
  ```

## Hardware Setup Reminder

```
ATtiny (TC2030) → ESP32 Feather V2
-----------------   ---------------
VDD (Pin 1)     →   3V3
GND (Pin 2)     →   GND
PA2 (LED pin)   →   GPIO16 (RX)
```

**Important:**
- PA2 is the LED pin on ATtiny, repurposed for UART TX
- LED must be disconnected or it will interfere with UART
- ESP32 GPIO16 is the hardware UART RX pin

## Python Script Options

```
usage: csv_logger.py [-h] [-b BAUD] [-l] [-a] [port] [output]

positional arguments:
  port                  Serial port (e.g., COM3 or /dev/ttyUSB0)
  output                Output CSV file (default: sensor_data.csv)

optional arguments:
  -h, --help            show this help message and exit
  -b, --baud BAUD       Baud rate (default: 115200)
  -l, --list            List available serial ports
  -a, --auto            Auto-detect ESP32 port
```

## Tips

1. **File naming**: Use descriptive names with dates:
   ```bash
   python csv_logger.py -a water_level_2025-03-15.csv
   ```

2. **Multiple sessions**: The script appends, so you can start/stop as needed

3. **Monitor while logging**: Open another terminal and tail the file:
   ```bash
   # Linux/Mac
   tail -f sensor_data.csv

   # Windows PowerShell
   Get-Content sensor_data.csv -Wait
   ```

4. **Calibration events**: The script logs calibration messages as comments:
   ```
   # CAL_START
   # CAL_OK
   ```

5. **Check data integrity**: Look for consistent timestamps (should increment by 1 each ~8 seconds)
