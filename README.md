# BMPCC4K ESP32 Controller

This project implements a Bluetooth controller for the Blackmagic Pocket Cinema Camera 4K using an ESP32 microcontroller. It provides a web interface for camera control and supports physical controls via a potentiometer and AS5600 encoder for focus adjustment.

## Features

- Bluetooth LE connection to BMPCC4K
- Web interface accessible via WiFi
- Support for up to 8 cameras
- Physical focus control with AS5600 encoder and potentiometer
- Camera settings control:
  - Focus (with auto focus)
  - ISO
  - Shutter speed/angle
  - White balance (with auto)
  - Tint
  - Aperture
  - Frame rate
  - Off-speed recording

## Hardware Requirements

- ESP32 DevKit 1
- AS5600 magnetic encoder
- Potentiometer (10kΩ recommended)
- Connecting wires

## Wiring Diagram

```
ESP32       AS5600      Potentiometer
3.3V   ---> VCC   
GND    ---> GND        GND
GPIO21 ---> SDA
GPIO22 ---> SCL
GPIO36 -----------------Signal
3.3V   -----------------VCC
```

## Building and Flashing

1. Install PlatformIO in Visual Studio Code
2. Clone this repository
3. Open the project in VS Code
4. Connect your ESP32
5. Click the "Build" button (✓) to compile
6. Click the "Upload" button (→) to flash
7. Click the "Upload Filesystem Image" in PlatformIO tasks to upload the web interface

## Usage

1. Power on the ESP32
2. Connect to the WiFi network "BMPCC-node" with password "esp32p4k"
3. Open a web browser and navigate to http://192.168.4.1
4. Turn on your BMPCC4K
5. Click "Connect" in the web interface
6. Enter the 6-digit PIN shown on the camera when prompted

## Focus Control

The focus can be controlled in three ways:
1. Web interface slider
2. AS5600 encoder
3. Potentiometer

Focus sensitivity and encoder degrees can be adjusted in the web interface under Settings.

## Development

This project uses:
- Arduino framework for ESP32
- NimBLE for Bluetooth LE
- ESPAsyncWebServer for the web interface
- AS5600 library for the magnetic encoder

## License

MIT License