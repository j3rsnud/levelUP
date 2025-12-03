# ESP32 UART Bridge for ATtiny UPDI Logging

This firmware turns an ESP32 Feather V2 (or compatible ESP32 board) into a UART bridge that receives data from the ATtiny's UPDI/PA0 pin and forwards it to USB Serial.

## Purpose

The ATtiny uses PA0 for both UPDI programming and as a button input. This bridge allows you to receive debug logs over the same pin after programming is complete, without requiring additional GPIO pins on the ATtiny.

## Hardware Setup

### Required Components
- ESP32 Feather V2 (or any ESP32 board with UART)
- TC2030 cable (6-pin tag-connect)
- ATtiny with UPDI logging firmware

### Wiring (TC2030 to ESP32)
```
TC2030 Pin → ESP32 Pin
-----------------------
VDD (Pin 1) → 3V3
GND (Pin 2) → GND
UPDI (Pin 6/PA0) → GPIO16 (RX)
```

**Important:** Do NOT connect both the UPDI programmer and ESP bridge simultaneously!

### Swapping Workflow
1. **Program mode:** Connect TC2030 to UPDI programmer, flash ATtiny
2. **Disconnect** programmer from TC2030
3. **Log mode:** Connect TC2030 to ESP bridge, monitor logs
4. Repeat as needed

## Software Setup

### Option 1: Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Install ESP32 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install
3. Open `esp_uart_bridge.ino`
4. Select your board: Tools → Board → ESP32 Arduino → Adafruit ESP32 Feather V2
5. Select port: Tools → Port → (your ESP32 port)
6. Upload: Sketch → Upload
7. Open Serial Monitor: Tools → Serial Monitor (115200 baud)

### Option 2: PlatformIO

1. Create `platformio.ini` in this directory (see example below)
2. Run: `pio run -t upload && pio device monitor`

Example `platformio.ini`:
```ini
[env:esp32feather]
platform = espressif32
board = adafruit_feather_esp32_v2
framework = arduino
monitor_speed = 115200
upload_speed = 921600
```

## Configuration

### Baud Rate
The default UART baud rate is **9600**, matching the ATtiny logger. To change:

1. **ATtiny side:** Modify `LOG_BAUD` in `include/log_updi_tx.h`
2. **ESP side:** Modify `UART_BAUD` in `esp_uart_bridge.ino`

Both must match for proper communication.

### RX Pin
Default is GPIO16. To use a different pin, modify `RX_PIN` in the sketch.

## Usage

1. Connect hardware as described above
2. Power on ATtiny (with ESP bridge connected)
3. Open Serial Monitor at 115200 baud
4. You should see:
   ```
   === ATtiny UPDI Logger Bridge ===
   Firmware: ESP32 UART Bridge v1.0
   USB Baud: 115200
   UART Baud: 9600
   RX Pin: GPIO16
   Listening for ATtiny data...

   t=1 c1=456 c2=789 c3=321
   t=2 c1=457 c2=790 c3=322
   ...
   ```

## Expected Output Format

The ATtiny sends sensor readings every 8 seconds in this format:
```
t=<timestamp> c1=<ch1_fF> c2=<ch2_fF> c3=<ch3_fF>
```

Where:
- `t`: Measurement count (increments every 8 seconds)
- `c1`, `c2`, `c3`: FDC1004 capacitance readings in femtofarads

## Troubleshooting

### No data received
- Check wiring (especially GND connection)
- Verify ATtiny is running (LED should blink on button press)
- Confirm baud rates match on both sides
- Check Serial Monitor is set to 115200 baud

### Garbled characters
- Baud rate mismatch - verify both sides use 9600
- Try slower baud (4800) if 9600 is unstable
- Check for loose connections

### ATtiny not responding
- Ensure UPDI programmer is disconnected
- Power cycle the ATtiny
- Verify ATtiny firmware has logging enabled (`LOG_ENABLED 1`)

## LED Indicator

The built-in LED (GPIO13) provides status:
- **Startup:** 3 quick blinks = bridge ready
- **Activity:** Brief flash when data received

## Future Enhancements

To enable two-way communication (send commands to ATtiny):
1. Uncomment the USB-to-UART forwarding code in `loop()`
2. Connect ESP TX to ATtiny PA0 (requires logic level matching)
3. Implement command parser on ATtiny side

## Specifications

- **USB Serial:** 115200 baud (PC connection)
- **UART Serial:** 9600 baud (ATtiny connection)
- **Protocol:** 8N1 (8 data bits, no parity, 1 stop bit)
- **Direction:** RX only (ATtiny → ESP → USB)
- **Power:** 3.3V (shared with ATtiny via TC2030)
