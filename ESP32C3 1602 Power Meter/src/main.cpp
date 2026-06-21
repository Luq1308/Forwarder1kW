#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int voltPin = 0; 
const int ampPin = 1;  
const int offPin = 2; // High = OFF
const int ccPin = 3;  // High = CC, Low = CV

// --- INTUITIVE CALIBRATION SECTION ---
const float MILLIVOLTS_PER_VOLT = 42.62; 
const float MILLIVOLTS_PER_AMP  = 96.30; 
// -------------------------------------

// Timing and Filtering Variables
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 5; // 5ms interval * 50 samples = 250ms window (4 Hz)
const int maxSamples = 50;

int sampleCount = 0;
uint32_t voltAccumulator = 0;
uint32_t ampAccumulator = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);
  
  // Initialize State Logic Pins
  pinMode(offPin, INPUT);
  pinMode(ccPin, INPUT);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // Step 1: Sample at precise, even intervals (every 5ms)
  if (currentMillis - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentMillis;

    voltAccumulator += analogReadMilliVolts(voltPin);
    ampAccumulator  += analogReadMilliVolts(ampPin);
    sampleCount++;

    // Step 2: Once we have accumulated 50 samples (exactly 250ms elapsed), update display
    if (sampleCount >= maxSamples) {
      
      // Calculate smooth averaged raw millivolts
      float avgVoltMV = (float)voltAccumulator / maxSamples;
      float avgAmpMV  = (float)ampAccumulator / maxSamples;

      // Reset accumulators immediately for the next window
      voltAccumulator = 0;
      ampAccumulator = 0;
      sampleCount = 0;

      // Step 3: Convert to raw units using initial constants
      float rawVoltage = avgVoltMV / 42.62;
      float rawCurrent = avgAmpMV / 96.30;

      // Step 3b: Apply Linear Regression Correction (y = mx + c)
      float actualVoltage = (rawVoltage * 0.958) + 0.205;
      float actualCurrent = (rawCurrent * 0.740) + 0.238;

      // Step 3c: Dead-zone Clamping
      if (rawVoltage < 0.05) actualVoltage = 0.00;
      if (rawCurrent < 0.05) actualCurrent = 0.00;

      float actualPower = actualVoltage * actualCurrent;

      // Step 4: Split into whole and fractional parts for integer zero-padding
      int voltWhole = (int)actualVoltage;
      int voltFrac  = (int)((actualVoltage - voltWhole) * 100);
      
      int ampWhole  = (int)actualCurrent;
      int ampFrac   = (int)((actualCurrent - ampWhole) * 100);

      int powerWhole = (int)actualPower;

      // Bound safety constraints to fit the LCD character widths
      if (powerWhole > 9999) powerWhole = 9999;
      if (voltWhole > 99) voltWhole = 99;
      if (ampWhole > 99) ampWhole = 99;

      // Step 5: Determine Output State (OFF / CC / CV)
      const char* statusIndicator;
      if (digitalRead(offPin) == HIGH) {
        statusIndicator = "OFF";
      } else {
        if (digitalRead(ccPin) == HIGH) {
          statusIndicator = " CC"; // Padded with a space for neat 3-char alignment
        } else {
          statusIndicator = " CV";
        }
      }

      // Step 6: Format and write to LCD
      char row1[17]; 
      char row2[17];

      // Row 1: "00.00V     0000W" (Exactly 5 spaces in the middle)
      sprintf(row1, "%02d.%02dV     %04dW", voltWhole, voltFrac, powerWhole);
      
      // Row 2: "00.00A       OFF" (Exactly 7 spaces in the middle)
      sprintf(row2, "%02d.%02dA       %3s", ampWhole, ampFrac, statusIndicator);

      lcd.setCursor(0, 0);
      lcd.print(row1);
      
      lcd.setCursor(0, 1);
      lcd.print(row2);
    }
  }
}