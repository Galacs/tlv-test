#include <Arduino.h>
#include "audioFile-32000fs-16bit.h" // mono audio file in flash
#include "tlv320aic31xx_codec.h"
#include <ESP_I2S.h>
#include <Wire.h>

// ESP32-S3 I2S bus settings
i2s_mode_t mode = I2S_MODE_STD; // Philips standard
i2s_data_bit_width_t width =
    I2S_DATA_BIT_WIDTH_16BIT;                // 16bit data/sample width
i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO; // 2 slots (stereo)
I2SClass i2s;

// background task continuously feeding DAC with audio data
void backgroundTask(void *parameter) {
  digitalWrite(LED_BUILTIN, HIGH);
  int idx = 0;
  while (true) {
    float phase = (2.0f * M_PI * 440.0f * idx) / (float)SAMPLERATE_HZ;
    int16_t sample = (int16_t)(32767.0f * sinf(phase));
    idx++;
    if (idx >= SAMPLERATE_HZ)
      idx = 0;

    uint8_t lo = sample & 0xFF;
    uint8_t hi = (sample >> 8) & 0xFF;

    i2s.write(lo);
    i2s.write(hi); // left
    i2s.write(lo);
    i2s.write(hi); // right
  }
  vTaskDelete(NULL);
}

void halt(const char *message) {
  Serial.println(message);
  while (true)
    yield(); // Function to halt on critical errors
}

TLV320AIC31xx codec(&Wire);

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  Wire.setPins(8, 7);
  Wire.begin();

  pinMode(9, INPUT_PULLDOWN);

  // Reset
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  delay(10);
  digitalWrite(16, HIGH);
  delay(100);

  codec.begin();
  codec.setWordLength(AIC31XX_WORD_LEN_16BITS);

  // Clocks
  codec.setCLKMUX(AIC31XX_PLL_CLKIN_BCLK, AIC31XX_CODEC_CLKIN_PLL);
  codec.setPLL(1, 2, 48, 0);   // 98.304 MHz PLL output
  codec.setPLLPower(true);
  delay(15);                    // wait for PLL lock

  codec.setNDACVal(6);
  codec.setNDACPower(true);
  codec.setMDACVal(4);
  codec.setMDACPower(true);
  codec.setDOSRVal(128);        // critical - was missing!

  // DAC
  codec.enableDAC();
  codec.setDACMute(false);
  codec.setDACVolume(0.0f, 0.0f);  // 0dB to start

  // Headphone output
  codec.enableHeadphoneAmp();
  codec.setHeadphoneMute(false);
  codec.setHeadphoneVolume(0.0f, 0.0f);  // 0dB
  codec.setHeadphoneGain(6.0f, 6.0f);

  // Speaker
  codec.enableSpeakerAmp();
  codec.setSpeakerMute(false);
  codec.setSpeakerGain(12.0f);
  codec.setSpeakerVolume(0.0f);

  // I2S - init AFTER codec so BCLK is present for PLL
  i2s.setPins(10, 11, 13);
  i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot);
  delay(50);  // let PLL lock to BCLK

// I2C scan
Serial.println("Scanning I2C...");
for (uint8_t addr = 1; addr < 127; addr++) {
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    Serial.printf("Found device at 0x%02X\n", addr);
  }
}

// Raw register dump - page 0
Serial.println("\n--- Raw Register Dump ---");
uint8_t addr = 0x18;

Wire.beginTransmission(addr);
Wire.write(0x00); Wire.write(0x00); // page 0
Wire.endTransmission();

Serial.println("-- Page 0 --");
for (uint8_t r = 0; r <= 70; r++) {
  Wire.beginTransmission(addr);
  Wire.write(r);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  Serial.printf("P0/R%-3d = 0x%02X\n", r, Wire.available() ? Wire.read() : 0xFF);
}

Wire.beginTransmission(addr);
Wire.write(0x00); Wire.write(0x01); // page 1
Wire.endTransmission();

Serial.println("-- Page 1 --");
for (uint8_t r = 0; r <= 40; r++) {
  Wire.beginTransmission(addr);
  Wire.write(r);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  Serial.printf("P1/R%-3d = 0x%02X\n", r, Wire.available() ? Wire.read() : 0xFF);
}

uint8_t a = 0x18;

auto wr = [&](uint8_t page, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(a);
  Wire.write(0x00); Wire.write(page);
  Wire.endTransmission();
  Wire.beginTransmission(a);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
};

// Route LEFT and RIGHT DAC to mixer (0xCC = both DACs to mixer)
wr(1, 35, 0xCC);

// HPL analog volume - 0dB (from working beep dump: 0xA5)
wr(1, 36, 0xA5);

// HPR analog volume - 0dB
wr(1, 37, 0xA5);

// HPL driver - powered, signal mode (from working beep dump: 0xD4)
wr(1, 31, 0xD4);

// HPR driver
wr(1, 32, 0xC6);

// Speaker volume (from working dump: 0x8C)
wr(1, 38, 0x8C);
wr(1, 39, 0x8C);
  xTaskCreate(backgroundTask, "bgTask", 4096, NULL, 1, NULL);
}

void loop() {
  sleep(1);
  Serial.print("HS Detect: ");
  Serial.println(codec.isHeadsetDetected());
}
