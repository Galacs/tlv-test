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
  digitalWrite(LED_BUILTIN, HIGH); // status LED On
  while (true) {
    for (uint32_t i = 0; i < sizeof(audioFile); i += 2) {
      uint8_t sample_low8bit = audioFile[i],
              sample_high8bit = audioFile[i + 1];
      // left channel, low 8 bits first
      i2s.write(sample_low8bit);
      i2s.write(sample_high8bit);
      // right channel, low 8 bits first
      i2s.write(sample_low8bit);
      i2s.write(sample_high8bit);
    }
  }
  vTaskDelete(NULL); // will never get here
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
  codec.setHeadphoneGain(0.0f, 0.0f);
  codec.setHeadphoneLineMode(true);

  // Speaker
  codec.enableSpeakerAmp();
  codec.setSpeakerMute(false);
  codec.setSpeakerGain(0.0f);
  codec.setSpeakerVolume(0.0f);

  // I2S - init AFTER codec so BCLK is present for PLL
  i2s.setPins(10, 11, 12);
  i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot);
  delay(50);  // let PLL lock to BCLK
  xTaskCreate(backgroundTask, "bgTask", 4096, NULL, 1, NULL);
}

void loop() {
  sleep(1);
  Serial.print("HS Detect: ");
  Serial.println(codec.isHeadsetDetected());
}
