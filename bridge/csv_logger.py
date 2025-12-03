#!/usr/bin/env python3
"""
CSV Logger for ATtiny Water Level Sensor

Reads CSV-formatted data from ESP32 bridge and logs to file.
Adds real timestamps and handles serial port connection.

Usage:
    python csv_logger.py [PORT] [OUTPUT_FILE]

Examples:
    python csv_logger.py COM3 sensor_data.csv
    python csv_logger.py /dev/ttyUSB0 data.csv
"""

import sys
import serial
import serial.tools.list_ports
from datetime import datetime
import argparse
import time


def list_serial_ports():
    """List all available serial ports"""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found!")
        return []

    print("\nAvailable serial ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")
    return [port.device for port in ports]


def find_esp32_port():
    """Try to auto-detect ESP32 port"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # ESP32 typically shows up with these descriptions
        if any(keyword in port.description.lower() for keyword in
               ['cp210', 'ch340', 'usb-serial', 'esp32', 'feather']):
            print(f"Auto-detected ESP32 on: {port.device}")
            return port.device
    return None


def open_serial_port(port, baud=115200, timeout=2):
    """Open serial port with error handling"""
    try:
        ser = serial.Serial(port, baud, timeout=timeout)
        print(f"✓ Connected to {port} at {baud} baud")
        return ser
    except serial.SerialException as e:
        print(f"✗ Error opening {port}: {e}")
        return None


def log_data(port, output_file, baud=115200):
    """Main logging function"""

    # Open serial port
    ser = open_serial_port(port, baud)
    if not ser:
        return

    # Open output file
    try:
        csv_file = open(output_file, 'a', buffering=1)  # Line buffered
        print(f"✓ Logging to: {output_file}")
    except IOError as e:
        print(f"✗ Error opening output file: {e}")
        ser.close()
        return

    print("\nStarting data capture...")
    print("Press Ctrl+C to stop\n")

    header_written = False
    line_count = 0

    try:
        while True:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='replace').strip()

                    if not line:
                        continue

                    # Pass through comment lines to console only
                    if line.startswith('#'):
                        print(line)
                        continue

                    # Handle CSV header
                    if line.startswith('timestamp,'):
                        if not header_written:
                            # Add real_time column to header
                            csv_file.write(f"real_time,{line}\n")
                            header_written = True
                            print(f"Header: real_time,{line}")
                        continue

                    # Check if line looks like CSV data (starts with a number)
                    if line and line[0].isdigit() or line.startswith('-'):
                        # Add real timestamp to data line
                        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
                        csv_line = f"{timestamp},{line}\n"
                        csv_file.write(csv_line)

                        line_count += 1
                        print(f"[{line_count:4d}] {timestamp} | {line}")
                    else:
                        # Unknown format - log as comment
                        print(f"? {line}")

                except UnicodeDecodeError:
                    print("⚠ Decode error - skipping malformed data")
                    continue

    except KeyboardInterrupt:
        print(f"\n\n✓ Stopped logging. Total lines: {line_count}")

    except serial.SerialException as e:
        print(f"\n✗ Serial port error: {e}")

    finally:
        csv_file.close()
        ser.close()
        print("✓ Files closed")


def main():
    parser = argparse.ArgumentParser(
        description='Log CSV data from ESP32 bridge to file',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python csv_logger.py COM3 data.csv        # Windows
  python csv_logger.py /dev/ttyUSB0 data.csv  # Linux
  python csv_logger.py -l                   # List available ports
  python csv_logger.py -a data.csv          # Auto-detect ESP32
        """
    )

    parser.add_argument('port', nargs='?', help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('output', nargs='?', default='sensor_data.csv',
                       help='Output CSV file (default: sensor_data.csv)')
    parser.add_argument('-b', '--baud', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('-l', '--list', action='store_true',
                       help='List available serial ports')
    parser.add_argument('-a', '--auto', action='store_true',
                       help='Auto-detect ESP32 port')

    args = parser.parse_args()

    # List ports mode
    if args.list:
        list_serial_ports()
        return

    # Auto-detect mode
    if args.auto:
        port = find_esp32_port()
        if not port:
            print("\n✗ Could not auto-detect ESP32. Available ports:")
            list_serial_ports()
            return
        log_data(port, args.output, args.baud)
        return

    # Manual port mode
    if not args.port:
        print("Error: Please specify a serial port or use -a for auto-detect")
        print("\nAvailable ports:")
        ports = list_serial_ports()
        if ports:
            print(f"\nExample: python csv_logger.py {ports[0]} sensor_data.csv")
        return

    log_data(args.port, args.output, args.baud)


if __name__ == '__main__':
    main()
