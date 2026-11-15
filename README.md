# Pulse-Oximeter-and-Temperature-Sensor--Vital-signs-monitor-# Portable Vital Signs Monitor (BPM, SpO2 & Skin Temperature)

A compact, low-cost biomedical monitoring prototype built around an **Arduino Nano** that simultaneously measures heart rate (BPM), peripheral oxygen saturation (SpO2), and contact skin temperature in real time on a local SSD1306 OLED display.

## Hardware & Pinout
* **MAX30102 Pulse Oximeter:** Connected via I2C (`SDA -> A4`, `SCL -> A5`, `VCC -> 3.3V/5V`, `GND -> GND`)
* **DS18B20 Digital Temperature Sensor:** Connected via 1-Wire (`DATA -> D2` with a 4.7 kΩ pull-up resistor to `5V`, `VCC -> 5V`, `GND -> GND`)
* **SSD1306 OLED Display (128×64):** Connected via I2C (`SDA -> A4`, `SCL -> A5`, `VCC -> 5V`, `GND -> GND`)

## Required Arduino Libraries
Install the following libraries via the Arduino IDE Library Manager before compiling:
1. `SparkFun MAX3010x Pulse and Proximity Sensor Library`
2. `OneWire`
3. `DallasTemperature`
4. `Adafruit GFX Library` & `Adafruit SSD1306`

## Demo Video
* **YouTube Demonstration:** [Watch Prototype Demo](https://youtu.be/XXXXXXXXXXX)

## Authors
* **Maryam Elassar** — Dept. of Electronics and Communication Engineering, AASTMT
* **Mahinour Aly** — Dept. of Electronics and Communication Engineering, AASTMT
