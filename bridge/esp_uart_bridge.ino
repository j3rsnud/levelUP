/**
 * @file esp_uart_bridge.ino
 * @brief ESP32 Feather V2 UART bridge for ATtiny UPDI logging
 *
 * Hardware connections (TC2030 to ESP):
 * - TC2030 VDD   → ESP 3V3
 * - TC2030 GND   → ESP GND
 * - TC2030 UPDI  → ESP GPIO RX (default: RX pin)
 *
 * This firmware parses UART data from ATtiny and outputs CSV format.
 * Converts from:
 *   "t=123 c1=456 c2=789 c3=321 c4=654"
 *   "dC: dc1=-50 dc2=-100 dc3=-200"
 * To CSV:
 *   "123,456,789,321,654,-50,-100,-200"
 *
 * Usage:
 * 1. Program ATtiny with UPDI programmer
 * 2. Disconnect programmer from TC2030
 * 3. Connect this ESP bridge to TC2030
 * 4. Open Serial Monitor at 115200 baud or run Python logger
 * 5. See CSV-formatted sensor data
 */

// Configuration
#define USB_BAUD 115200    // USB Serial baud rate (PC connection)
#define UART_BAUD 9600     // UART baud rate (must match ATtiny LOG_BAUD)

// ESP32 Feather V2 pins
#define RX_PIN 16          // GPIO16 - RX from ATtiny UPDI/PA0
// TX not used (RX-only bridge)

// Optional: LED for activity indication
#define LED_BUILTIN 13     // Built-in LED on most ESP32 boards

// Data buffer for sensor readings
struct SensorData {
  int timestamp;
  int c1, c2, c3, c4;
  bool has_raw;
} sensorData;

struct DriftData {
  int dc1, dc2, dc3;
  bool has_drift;
} driftData;

void setup() {
  // Initialize data buffers
  sensorData.has_raw = false;
  driftData.has_drift = false;

  // Initialize USB Serial (for PC monitoring)
  Serial.begin(USB_BAUD);
  while (!Serial) {
    delay(10);  // Wait for USB Serial to connect
  }

  Serial.println("# ATtiny UPDI Logger Bridge - CSV Mode");
  Serial.println("# Firmware: ESP32 UART Bridge v2.0");
  Serial.println("# USB Baud: " + String(USB_BAUD));
  Serial.println("# UART Baud: " + String(UART_BAUD));
  Serial.println("# RX Pin: GPIO" + String(RX_PIN));
  Serial.println("# Format: timestamp,c1,c2,c3,c4,dc1,dc2,dc3");
  Serial.println("# Listening for ATtiny data...");

  // Output CSV header
  Serial.println("timestamp,c1,c2,c3,c4,dc1,dc2,dc3");

  // Initialize UART (from ATtiny)
  // ESP32: Serial1.begin(baud, mode, rxPin, txPin)
  Serial1.begin(UART_BAUD, SERIAL_8N1, RX_PIN, -1);  // -1 = no TX pin

  // Optional: Initialize LED for activity indication
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Flash LED to indicate ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

/**
 * Parse raw sensor line: "t=123 c1=456 c2=789 c3=321 c4=654"
 */
void parseSensorLine(String line) {
  // Parse: t=123 c1=456 c2=789 c3=321 c4=654
  int t_idx = line.indexOf("t=");
  int c1_idx = line.indexOf("c1=");
  int c2_idx = line.indexOf("c2=");
  int c3_idx = line.indexOf("c3=");
  int c4_idx = line.indexOf("c4=");

  if (t_idx != -1 && c1_idx != -1 && c2_idx != -1 && c3_idx != -1 && c4_idx != -1) {
    sensorData.timestamp = line.substring(t_idx + 2, line.indexOf(' ', t_idx)).toInt();
    sensorData.c1 = line.substring(c1_idx + 3, line.indexOf(' ', c1_idx)).toInt();
    sensorData.c2 = line.substring(c2_idx + 3, line.indexOf(' ', c2_idx)).toInt();
    sensorData.c3 = line.substring(c3_idx + 3, line.indexOf(' ', c3_idx)).toInt();
    sensorData.c4 = line.substring(c4_idx + 3).toInt();
    sensorData.has_raw = true;
  }
}

/**
 * Parse drift-corrected line: "dC: dc1=-50 dc2=-100 dc3=-200"
 */
void parseDriftLine(String line) {
  // Parse: dC: dc1=-50 dc2=-100 dc3=-200
  int dc1_idx = line.indexOf("dc1=");
  int dc2_idx = line.indexOf("dc2=");
  int dc3_idx = line.indexOf("dc3=");

  if (dc1_idx != -1 && dc2_idx != -1 && dc3_idx != -1) {
    driftData.dc1 = line.substring(dc1_idx + 4, line.indexOf(' ', dc1_idx)).toInt();
    driftData.dc2 = line.substring(dc2_idx + 4, line.indexOf(' ', dc2_idx)).toInt();
    driftData.dc3 = line.substring(dc3_idx + 4).toInt();
    driftData.has_drift = true;
  }
}

/**
 * Output combined CSV line if we have both sensor and drift data
 */
void outputCSV() {
  if (sensorData.has_raw && driftData.has_drift) {
    // Output: timestamp,c1,c2,c3,c4,dc1,dc2,dc3
    Serial.print(sensorData.timestamp);
    Serial.print(',');
    Serial.print(sensorData.c1);
    Serial.print(',');
    Serial.print(sensorData.c2);
    Serial.print(',');
    Serial.print(sensorData.c3);
    Serial.print(',');
    Serial.print(sensorData.c4);
    Serial.print(',');
    Serial.print(driftData.dc1);
    Serial.print(',');
    Serial.print(driftData.dc2);
    Serial.print(',');
    Serial.println(driftData.dc3);

    // Reset flags for next reading
    sensorData.has_raw = false;
    driftData.has_drift = false;

    // Blink LED on successful CSV output
    digitalWrite(LED_BUILTIN, HIGH);
    delay(10);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void loop() {
  // Read line-by-line from ATtiny UART
  if (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    line.trim();  // Remove whitespace

    // Skip empty lines
    if (line.length() == 0) {
      return;
    }

    // Debug: pass through unknown lines as comments
    if (line.startsWith("hello") || line.startsWith("CAL_")) {
      Serial.println("# " + line);
      return;
    }

    // Parse based on line format
    if (line.startsWith("t=")) {
      parseSensorLine(line);
    } else if (line.startsWith("dC:")) {
      parseDriftLine(line);
    } else {
      // Unknown format - output as comment for debugging
      Serial.println("# UNKNOWN: " + line);
    }

    // Try to output CSV if we have complete data
    outputCSV();
  }
}
