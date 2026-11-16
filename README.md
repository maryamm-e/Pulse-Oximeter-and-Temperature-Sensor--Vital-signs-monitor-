# Portable Vital Signs Monitor (BPM, SpO2 & Skin Temperature)

## Project Description
This project is a compact, low-cost biomedical vital signs monitor built around an **Arduino Nano (ATmega328P)** microcontroller. It simultaneously measures heart rate (BPM), peripheral oxygen saturation (SpO2), and contact skin temperature in real time, displaying all readings locally on an integrated 0.96-inch SSD1306 OLED screen without requiring a smartphone, computer, or internet connection. Optical photoplethysmography (PPG) signals from the MAX30102 sensor are smoothed using a moving-average filter to suppress hand-motion noise, while the DS18B20 digital thermometer applies a +0.35°C skin calibration offset for clinical-grade accuracy.

## Prototype Photo
<img width="500" height="1000" alt="WhatsApp Image 2026-11-15 at 1 24 14 PM" src="https://github.com/user-attachments/assets/d2c0974c-8d8f-443c-9a4a-7210b5c9d21c" />


## Component List (Bill of Materials)
| Component | Model / Specification | Qty |
| :--- | :--- | :---: |
| Microcontroller | Arduino Nano (ATmega328P, 5V) | 1 |
| Pulse Oximeter & Heart Rate Sensor | MAX30102 Optical Sensor Module | 1 |
| Digital Temperature Sensor | DS18B20 (with 4.7 kΩ pull-up resistor) | 1 |
| OLED Display | 0.96" SSD1306 (128×64 pixels, I2C) | 1 |
| Power Supply | 5V Regulated DC Supply / Battery Pack + Switch | 1 |
| Wiring & Enclosure | Jumper Wires & Custom Portable Enclosure | 1[cite: 8, 9] |

## Wiring & Pinout
| Component Pin | Arduino Nano Pin | Interface |
| :--- | :--- | :--- |
| **MAX30102** `VIN` / `GND` | `5V` (or `3.3V`) / `GND` | Power |
| **MAX30102** `SDA` / `SCL` | `A4` / `A5` | I2C |
| **SSD1306 OLED** `VCC` / `GND` | `5V` / `GND` | Power |
| **SSD1306 OLED** `SDA` / `SCL` | `A4` / `A5` | I2C |
| **DS18B20** `VCC` / `GND` | `5V` / `GND` | Power |
| **DS18B20** `DATA` | `D2` *(with 4.7 kΩ resistor to `5V`)* | 1-Wire[cite: 8, 9] |

## Required Arduino Libraries
Install the following libraries in the Arduino IDE (**Sketch → Include Library → Manage Libraries**):
* **SparkFun MAX3010x Pulse and Proximity Sensor Library** (`v1.1.2` or newer)
* **OneWire** (`v2.3.7` or newer)
* **DallasTemperature** (`v3.9.0` or newer)
* **Adafruit SSD1306** (`v2.5.7` or newer)
* **Adafruit GFX Library** (`v1.11.5` or newer)

## How to Upload and Run the Code
1. Download `vital_signs_monitor.ino` from this repository and open it in the **Arduino IDE**.
2. Install all five libraries listed above via the **Library Manager**.
3. Connect the Arduino Nano to your computer via USB.
4. Go to **Tools → Board** and select **Arduino Nano** (if uploading fails, select **Tools → Processor → ATmega328P (Old Bootloader)**).
5. Select your active **Port** under **Tools → Port** and click **Upload** (`→`).
6. Once uploaded, place your index finger gently over the MAX30102 sensor while holding the DS18B20 probe against your skin—live BPM, SpO2, and temperature readings will appear on the OLED screen within 4 seconds[cite: 9].

## Authors
* **Maryam Elassar** — Dept. of Electronics and Communication Engineering, AASTMT
* **Mahinour Aly** — Dept. of Electronics and Communication Engineering, AASTMT[cite: 8]
