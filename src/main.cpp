#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>

I2SClass i2s;

#define CODEC_ADDR 0x18
#define RESET_PIN 16  // adjust if different

void wr(uint8_t page, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(CODEC_ADDR);
  Wire.write(0x00); Wire.write(page);
  Wire.endTransmission();
  Wire.beginTransmission(CODEC_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.setPins(8, 7);
  Wire.begin();

  // Hardware reset
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(10);
  digitalWrite(RESET_PIN, HIGH);
  delay(100);

  // Start I2S FIRST so BCLK is running before PLL config
  i2s.setPins(10, 11, 12, -1, -1);
  i2s.begin(I2S_MODE_STD, 32000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  Serial.println("I2S started");
  delay(100); // let BCLK stabilize

  // Minimal register writes - nothing else
  wr(0,  4, 0x03); // CODEC_CLKIN = PLL
  wr(0,  4, 0x07); // PLL_CLKIN = BCLK, CODEC_CLKIN = PLL
  // wr(0,  6, 0x48); // J=48 for PLL
  wr(0, 6, 0x28); // BCLK divider off, codec slave, default value
  wr(0,  5, 0x92); // PLL power up, P=1, R=2
  delay(15);        // PLL lock time

  wr(0, 11, 0x86); // NDAC=6, powered
  wr(0, 12, 0x84); // MDAC=4, powered
  wr(0, 13, 0x00); // DOSR=128 MSB
  wr(0, 14, 0x80); // DOSR=128 LSB

  wr(0, 27, 0x00); // I2S, 16bit, slave
  wr(0, 61, 0x01); // PRB_P1
  wr(0, 63, 0xD6); // Both DACs on, L→L, R→R
  wr(0, 64, 0x00); // Unmute
  wr(0, 65, 0x00); // Left DAC 0dB
  wr(0, 66, 0x00); // Right DAC 0dB

  // Page 1 analog - copied exactly from working beep dump
  wr(1, 31, 0xD4); // HPL driver
  wr(1, 32, 0xC6); // HPR driver
  wr(1, 33, 0x3E); // HP PGA
  wr(1, 35, 0xCC); // DAC→mixer
  wr(1, 36, 0xA5); // HPL volume
  wr(1, 37, 0xA5); // HPR volume
  wr(1, 38, 0x8C); // SPK L
  wr(1, 39, 0x8C); // SPK R
  wr(1, 40, 0x07); // SPK driver
  // Force BCLK and WCLK as inputs (codec slave mode)
  wr(0, 27, 0x00); // bit3=0 BCLK input, bit2=0 WCLK input
  wr(0, 29, 0x00); // BCLK divider - make sure not generating BCLK

  Serial.println("Codec configured");

  // Generate sine wave
  xTaskCreate([](void*) {
    int idx = 0;
    while(true) {
      float phase = (2.0f * M_PI * 440.0f * idx) / 32000.0f;
      int16_t s = (int16_t)(32767.0f * sinf(phase));
      if (++idx >= 32000) idx = 0;
      uint8_t lo = s & 0xFF;
      uint8_t hi = (s >> 8) & 0xFF;
      i2s.write(lo); i2s.write(hi);
      i2s.write(lo); i2s.write(hi);
    }
  }, "audio", 4096, NULL, 1, NULL);
}

void loop() { delay(1000); }