#define USE_UTF8_LONG_NAMES 1
#include <Arduino.h>
#include "tlv320aic31xx_codec.h"
// #include <ESP_I2S.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Disk/AudioSourceSD.h"
#include <SPI.h>
#include <Wire.h>
#include <Button2.h>

Button2 btn_a;
Button2 btn_b;

SPIClass *hspi = NULL;

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSD source(startFilePath, ext, 36, hspi);
MP3DecoderHelix decoder;
I2SStream i2s;
AudioPlayer player(source, i2s, decoder);

// background task continuously feeding DAC with audio data
// void backgroundTask(void *parameter) {
//   digitalWrite(LED_BUILTIN, HIGH); // status LED On
//   while (true) {
//     for (uint32_t i = 0; i < sizeof(audioFile); i += 2) {
//       uint8_t sample_low8bit = audioFile[i],
//               sample_high8bit = audioFile[i + 1];
//       // left channel, low 8 bits first
//       i2s.write(sample_low8bit);
//       i2s.write(sample_high8bit);
//       // right channel, low 8 bits first
//       i2s.write(sample_low8bit);
//       i2s.write(sample_high8bit);
//     }
//   }
//   vTaskDelete(NULL); // will never get here
// }

void halt(const char *message) {
  Serial.println(message);
  while (true)
    yield(); // Function to halt on critical errors
}

TLV320AIC31xx codec(&Wire);

void printMetaData(MetaDataType type, const char *str, int len) {
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void next_song(Button2 &btn) {
  player.next();
}

void prev_song(Button2 &btn) {
  player.previous();
}

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  Wire.setPins(8, 7);
  Wire.begin();

  btn_a.begin(4);
  btn_b.begin(3);

  btn_a.setClickHandler(prev_song);
  btn_b.setClickHandler(next_song);

  hspi = new SPIClass(HSPI);
  hspi->begin(34, 33, 35);
  if (!SD.begin(36, *hspi, 4000000)) { // try 4MHz first
    Serial.println("SD init failed!");
    // print hspi->pin details here
  } else {
    Serial.println("SD OK");
  }
  // delay(1000000);

  // pinMode(9, INPUT_PULLDOWN);

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
  codec.setPLL(1, 2, 48, 0); // 98.304 MHz PLL output
  codec.setPLLPower(true);
  delay(15); // wait for PLL lock

  codec.setNDACVal(6);
  codec.setNDACPower(true);
  codec.setMDACVal(4);
  codec.setMDACPower(true);
  codec.setDOSRVal(128); // critical - was missing!

  // DAC
  codec.enableDAC();
  codec.setDACMute(false);
  codec.setDACVolume(0.0f, 0.0f); // 0dB to start

  // Headphone output
  codec.enableHeadphoneAmp();
  codec.setHeadphoneMute(false);
  codec.setHeadphoneVolume(-35.0f, -35.0f); // 0dB
  codec.setHeadphoneGain(0.0f, 0.0f);
  codec.setHeadphoneLineMode(true);

  // Speaker
  codec.enableSpeakerAmp();
  codec.setSpeakerMute(false);
  codec.setSpeakerGain(0.0f);
  codec.setSpeakerVolume(0.0f);

  // setup player
  // source.setFileFilter("*Bob Dylan*");
  player.setBufferSize(1024);
  // player.setMetadataCallback(printMetaData);
  player.begin();
  // I2S - init AFTER codec so BCLK is present for PLL
  // i2s.setPins(10, 11, 12);
  // i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot);
  delay(50); // let PLL lock to BCLK
             // xTaskCreate(backgroundTask, "bgTask", 4096, NULL, 1, NULL);
  Serial.println("starting I2S...");
  auto config = i2s.defaultConfig(TX_MODE);
  // Custom I2S output pins
  config.pin_bck = 10;
  config.pin_ws = 11;
  config.pin_data = 12;
  config.buffer_size = 1024;   // add this
  config.buffer_count = 16;
  i2s.begin(config);

  // Setup sine wave
  Serial.println("started...");
}

void loop() {
  if (i2s.availableForWrite() > 0) {
    player.copy();
  }
  delay(1);
  btn_a.loop();
  btn_b.loop();
}
