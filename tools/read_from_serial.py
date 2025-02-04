#!/usr/bin/env python3

# Basic utility to read from serial port

import serial

# Serial connection configuration
port = "/dev/cu.usbmodem1101"
baudrate = 115200

serial_connection = serial.Serial(port, baudrate)
destination_file = open("../serial_data.f32", "wb")

# Read data
while True:
    data = serial_connection.read(128)
    if data == b"EOF":
        break
    print(data)
    destination_file.write(data)

# Close the file and serial connection
destination_file.close()
serial_connection.close()