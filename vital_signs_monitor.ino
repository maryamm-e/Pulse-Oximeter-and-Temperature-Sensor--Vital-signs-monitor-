#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <math.h>

// =====================================
// CONFIGURATION
// =====================================

#define MAX_ADDR 0x57
#define OLED_ADDR 0x3C
#define DS_PIN 3

#define FINGER_THRESHOLD 50000UL
#define SAMPLE_RATE 100

// =====================================
// DS18B20
// =====================================

OneWire oneWire(DS_PIN);
DallasTemperature ds(&oneWire);

float temperature = DEVICE_DISCONNECTED_C;
unsigned long tempStart = 0;
bool tempPending = false;

// =====================================
// OLED
// =====================================

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(
  U8G2_R0, U8X8_PIN_NONE
);

// =====================================
// MAX30102 VARIABLES
// =====================================

bool maxOK = false;
bool finger = false;

uint32_t rawIR = 0;
uint32_t rawRED = 0;

float dcIR = 0;
float dcRED = 0;

float filtered = 0;
float prev1 = 0;
float prev2 = 0;
float envelope = 0;

uint32_t sampleIndex = 0;
uint32_t lastBeatSample = 0;

float bpm = 0;
bool validBPM = false;

float irPower = 0;
float redPower = 0;
uint16_t oxygenSamples = 0;

int oxygen = 0;
bool validOxygen = false;

unsigned long lastDisplay = 0;
unsigned long lastPoll = 0;

// =====================================
// CUSTOM MAX30102 I2C DRIVER
// =====================================

bool writeMAX(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(reg);
  Wire.write(value);

  return Wire.endTransmission() == 0;
}

bool readMAX(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom(
      (uint8_t)MAX_ADDR,
      (uint8_t)1) != 1)
    return false;

  value = Wire.read();
  return true;
}

// Initialize MAX30102 without external library
bool initMAX30102() {
  uint8_t chipID = 0;

  if (!readMAX(0xFF, chipID))
    return false;

  Serial.print(F("CHIP ID: 0x"));
  Serial.println(chipID, HEX);

  if (chipID != 0x15)
    return false;

  // Reset sensor
  if (!writeMAX(0x09, 0x40))
    return false;

  delay(20);

  // Disable interrupts
  writeMAX(0x02, 0x00);
  writeMAX(0x03, 0x00);

  // Clear FIFO
  writeMAX(0x04, 0x00);
  writeMAX(0x05, 0x00);
  writeMAX(0x06, 0x00);

  // FIFO rollover enabled
  // No sample averaging
  writeMAX(0x08, 0x10);

  // ADC range 4096 nA
  // Sample rate 100 Hz
  // Pulse width 411 us
  writeMAX(0x0A, 0x27);

  // Red LED current
  writeMAX(0x0C, 0x24);

  // IR LED current
  writeMAX(0x0D, 0x24);

  // SpO2 mode: RED + IR
  if (!writeMAX(0x09, 0x03))
    return false;

  return true;
}

// Read one RED + IR sample from FIFO
bool readMAXSample(
  uint32_t &red,
  uint32_t &ir
) {
  uint8_t writePtr;
  uint8_t readPtr;

  if (!readMAX(0x04, writePtr))
    return false;

  if (!readMAX(0x06, readPtr))
    return false;

  // No unread samples
  if ((writePtr & 0x1F) ==
      (readPtr & 0x1F))
    return false;

  Wire.beginTransmission(MAX_ADDR);
  Wire.write(0x07);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom(
      (uint8_t)MAX_ADDR,
      (uint8_t)6) != 6)
    return false;

  red = (uint32_t)Wire.read() << 16;
  red |= (uint32_t)Wire.read() << 8;
  red |= Wire.read();

  ir = (uint32_t)Wire.read() << 16;
  ir |= (uint32_t)Wire.read() << 8;
  ir |= Wire.read();

  // Sensor uses 18-bit ADC data
  red &= 0x3FFFF;
  ir &= 0x3FFFF;

  return true;
}

// =====================================
// RESET SIGNAL PROCESSING
// =====================================

void resetPulse() {
  sampleIndex = 0;
  lastBeatSample = 0;

  dcIR = 0;
  dcRED = 0;

  filtered = 0;
  prev1 = 0;
  prev2 = 0;
  envelope = 0;

  bpm = 0;
  validBPM = false;

  irPower = 0;
  redPower = 0;
  oxygenSamples = 0;

  oxygen = 0;
  validOxygen = false;
}

// =====================================
// CUSTOM HEART RATE + SpO2 ESTIMATION
// =====================================

void processPulse(
  uint32_t red,
  uint32_t ir
) {
  rawIR = ir;
  rawRED = red;

  bool hasFinger =
    ir > FINGER_THRESHOLD;

  if (!hasFinger) {
    finger = false;
    resetPulse();
    return;
  }

  if (!finger) {
    resetPulse();

    dcIR = ir;
    dcRED = red;
    finger = true;
  }

  sampleIndex++;

  // Estimate DC baseline
  float irDiff = (float)ir - dcIR;
  float redDiff = (float)red - dcRED;

  dcIR += irDiff * 0.01f;
  dcRED += redDiff * 0.01f;

  // Remove DC and smooth signal
  float acIR = (float)ir - dcIR;
  float acRED = (float)red - dcRED;

  filtered =
    filtered * 0.75f + acIR * 0.25f;

  // Track signal amplitude
  envelope =
    envelope * 0.995f +
    fabs(filtered) * 0.005f;

  // Find positive pulse peaks
  bool peak =
    prev1 > prev2 &&
    prev1 > filtered &&
    prev1 > envelope * 1.4f &&
    prev1 > 70.0f;

  if (peak && sampleIndex > 100) {

    uint32_t elapsed =
      sampleIndex - lastBeatSample;

    if (lastBeatSample == 0) {
      lastBeatSample = sampleIndex;
    }
    else if (elapsed >= 35 &&
             elapsed <= 150) {

      float newBPM =
        (60.0f * SAMPLE_RATE) / elapsed;

      if (!validBPM) {
        bpm = newBPM;
      } else {
        bpm = bpm * 0.7f +
              newBPM * 0.3f;
      }

      validBPM = true;
      lastBeatSample = sampleIndex;
    }
  }

  prev2 = prev1;
  prev1 = filtered;

  // No recent pulse detected
  if (lastBeatSample != 0 &&
      sampleIndex - lastBeatSample > 300) {
    validBPM = false;
    bpm = 0;
    lastBeatSample = 0;
  }

  // Experimental SpO2 ratio estimation
  // Accumulate approximately 4 seconds
  if (sampleIndex > 100) {
    irPower += acIR * acIR;
    redPower += acRED * acRED;
    oxygenSamples++;
  }

  if (oxygenSamples >= 400) {
    float irRMS =
      sqrt(irPower / oxygenSamples);

    float redRMS =
      sqrt(redPower / oxygenSamples);

    if (irRMS > 40 &&
        redRMS > 40 &&
        dcIR > 10000 &&
        dcRED > 10000) {

      float ratio =
        (redRMS / dcRED) /
        (irRMS / dcIR);

      float estimate =
        110.0f - 25.0f * ratio;

      if (ratio >= 0.3f &&
          ratio <= 1.6f &&
          estimate >= 70 &&
          estimate <= 100) {

        oxygen = (int)(estimate + 0.5f);
        validOxygen = true;
      } else {
        validOxygen = false;
      }
    } else {
      validOxygen = false;
    }

    irPower = 0;
    redPower = 0;
    oxygenSamples = 0;
  }
}

// =====================================
// TEMPERATURE
// =====================================

void updateTemperature() {
  unsigned long now = millis();

  if (tempPending &&
      now - tempStart >= 200) {

    temperature =
      ds.getTempCByIndex(0);

    tempPending = false;
  }

  if (!tempPending &&
      now - tempStart >= 1000) {

    ds.requestTemperatures();
    tempStart = now;
    tempPending = true;
  }
}

// =====================================
// OLED DISPLAY - 128x64 VITAL SIGNS UI
// =====================================

// These are optional DISPLAY thresholds, not diagnostic limits.
#define HIGH_TEMP_C 38.0f
#define LOW_OXYGEN_ESTIMATE 95
#define FAST_PULSE_BPM 100.0f
#define SLOW_PULSE_BPM 60.0f

const char* feedbackMessage() {
  if (temperature == DEVICE_DISCONNECTED_C) return "TEMP SENSOR ERROR";
  if (temperature >= HIGH_TEMP_C) return "HIGH TEMP - CHECK";
  if (!maxOK) return "PULSE SENSOR ERROR";
  if (!finger) return "PLACE FINGER";
  if (validOxygen && oxygen < LOW_OXYGEN_ESTIMATE)
    return "LOW O2 EST. (DEMO)";
  if (validBPM && bpm > FAST_PULSE_BPM)
    return "FAST PULSE (DEMO)";
  if (validBPM && bpm < SLOW_PULSE_BPM)
    return "SLOW PULSE (DEMO)";
  if (!validBPM || !validOxygen) return "MEASURING...";
  return "READINGS ACTIVE";
}

void updateDisplay() {
  char bpmText[8];
  char oxygenText[8];
  char tempText[14];

  if (finger && validBPM && bpm > 0 && bpm < 250) {
    itoa((int)(bpm + 0.5f), bpmText, 10);
  } else {
    strcpy(bpmText, "--");
  }

  if (finger && validOxygen && oxygen >= 70 && oxygen <= 100) {
    itoa(oxygen, oxygenText, 10);
  } else {
    strcpy(oxygenText, "--");
  }

  if (temperature == DEVICE_DISCONNECTED_C) {
    strcpy(tempText, "ERR");
  } else {
    char numeric[10];
    dtostrf(temperature, 0, 1, numeric);
    snprintf(tempText, sizeof(tempText), "%s C", numeric);
  }

  // One-page buffer keeps RAM usage low on Arduino Nano.
  oled.firstPage();
  do {
    oled.setDrawColor(1);

    // Header: exact requested title
    oled.setFont(u8g2_font_5x7_tr);
    const char* title = "VITAL SIGNS MONITORING";
    int titleX = (128 - oled.getStrWidth(title)) / 2;
    oled.drawStr(titleX, 7, title);
    oled.drawHLine(0, 9, 128);

    // Two large-value panels
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(19, 19, "BPM");
    oled.drawStr(78, 19, "SpO2~");
    oled.drawVLine(64, 11, 31);

    // Large pulse and oxygen readings
    oled.setFont(u8g2_font_logisoso18_tn);
    int w = oled.getStrWidth(bpmText);
    oled.drawStr(32 - w / 2, 41, bpmText);

    w = oled.getStrWidth(oxygenText);
    int oxygenX = 96 - (w + 7) / 2;
    oled.drawStr(oxygenX, 41, oxygenText);
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(oxygenX + w + 1, 37, "%");

    // Temperature - one line under large values
    oled.drawHLine(0, 43, 128);
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(3, 52, "TEMP");
    oled.setFont(u8g2_font_helvB12_tr);
    oled.drawStr(40, 54, tempText);

    // Inverted feedback strip
    oled.drawBox(0, 56, 128, 8);
    oled.setDrawColor(0);
    oled.setFont(u8g2_font_5x7_tr);
    const char* status = feedbackMessage();
    int statusX = (128 - oled.getStrWidth(status)) / 2;
    oled.drawStr(statusX, 63, status);
    oled.setDrawColor(1);
  } while (oled.nextPage());
}

// =====================================
// SERIAL MONITOR
// =====================================

void printSerial() {
  Serial.println(F("----------------"));

  Serial.print(F("Temperature: "));
  Serial.println(temperature);

  Serial.print(F("RED: "));
  Serial.println(rawRED);

  Serial.print(F("IR: "));
  Serial.println(rawIR);

  Serial.print(F("Finger: "));
  Serial.println(
    finger ? F("YES") : F("NO")
  );

  Serial.print(F("BPM: "));

  if (validBPM) {
    Serial.println(bpm);
  } else {
    Serial.println(F("Waiting"));
  }

  Serial.print(F("SpO2 estimate: "));

  if (validOxygen) {
    Serial.print(oxygen);
    Serial.println(F("%"));
  } else {
    Serial.println(F("Waiting"));
  }
}

// =====================================
// SETUP
// =====================================

void setup() {
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(100000);

  // OLED
  oled.setI2CAddress(OLED_ADDR << 1);
  oled.begin();
  // Initial screen uses the same compact dashboard.
  // The SSD1306 is redrawn in updateDisplay().
  updateDisplay();

  // DS18B20
  ds.begin();
  ds.setResolution(10);
  ds.setWaitForConversion(false);

  ds.requestTemperatures();
  tempStart = millis();
  tempPending = true;

  // MAX30102 custom initialization
  maxOK = initMAX30102();

  if (maxOK) {
    Serial.println(F("MAX30102 READY"));
  } else {
    Serial.println(F("MAX30102 ERROR"));
  }

  Wire.setClock(100000);
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  updateTemperature();

  // Check FIFO regularly
  if (maxOK && millis() - lastPoll >= 5) {
    lastPoll = millis();

    uint32_t red;
    uint32_t ir;

    // Process queued samples
    for (uint8_t i = 0; i < 24; i++) {
      if (!readMAXSample(red, ir))
        break;

      processPulse(red, ir);
    }
  }

  // Update display every second
  if (millis() - lastDisplay >= 1000) {
    lastDisplay = millis();

    updateDisplay();
    printSerial();
  }
}
