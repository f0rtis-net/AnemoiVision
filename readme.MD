# AnemoiVision

**AnemoiVision** is an embedded weather station project running on **ESP32-S3** that collects environmental data and predicts short-term weather trends using a lightweight neural network. The project combines IoT sensors, FreeRTOS multitasking, and on-device machine learning to provide real-time insights without relying on external servers.

## Features

- Real-time data collection from multiple sensors:
  - Temperature, humidity, and pressure (BME280)
  - Rainfall / environmental sensors

- **ESP32-S3-based MCU** with dual-core performance and hardware acceleration for neural network inference.

- Lightweight neural network for **short-term weather prediction**.

- FreeRTOS-based multitasking:
  - Separate tasks for data acquisition, prediction, and communication.

- Flexible bus and driver architecture:
  - Supports I2C, SPI, GPIO, and other buses.
  - Modular drivers for easy expansion.

- Optional display integration (OLED/LCD) for real-time readings.
