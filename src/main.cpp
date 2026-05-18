#include <Arduino.h>
#include <ESP_I2S.h>
// #include "TLV320DAC3101.h"
#include "audioFile_16000Hz_16bit_mono.h"   // audio file in flash
#include <Adafruit_TLV320DAC3100.h>

Adafruit_TLV320DAC3100 codec; // Create codec object

// ESP32-S3 I2S settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)
I2SClass i2s;

// TLV320DAC3101 dac;
// tlv320_init_config_t cfg;

#define pBCLK  10   // BITCLOCK - I2S clock
#define pWS    1  // LRCLOCK - Word select
#define pDOUT  13  // DATA - I2S data
#define pRESET 16   // RST - hardware reset (required for Pico 2W / RP2350)

// helper function to halt on critical errors
void halt(const char *message) {
  Serial.println(message);
  while (true) yield();
}

void setup() {
  Serial.begin(115200);
  Wire.setPins(8, 7);
  delay(100);

  // Serial.println("\nrunning example \"Play Audio From Flash\":");

  // HW reset makes sure DAC chip is reset properly
  pinMode(pRESET, OUTPUT);
  digitalWrite(pRESET, LOW);
  delay(10);
  digitalWrite(pRESET, HIGH);

  // // TLV320DAC3101 Audio DAC initialization
  // cfg.sample_frequency = SAMPLERATE_HZ;      // Hz, must be set
  // cfg.dac_gain_left = -15.0;                 // dB, defaults to 0dB when not set,
  // cfg.dac_gain_right = -15.0;                // allowed range: -63.5...+24.0 dB

  // if (!dac.initDAC(&cfg)) {
  //   halt(dac.getLastError().c_str());
  // }

  // if (!dac.setDACProcessingBlock(1)) {   // PRB_P1 – simple stereo pass‑through
  //   halt("Failed to configure processing block!");
  // }

  // // activate headphone output and set headphone volume
  // if (!dac.configHeadphoneOutput(true,              // headphone output enabled
  //                                false,             // HP(L/R) output driver acts as headphone driver
  //                                80)) {             // set volume (allowed range: 0(quiet)...127(loud))
  //   halt("Failed to configure headphone output!");
  // }

  // // activate speaker output and set speaker volume
  // if (!dac.configSpeakerOutput(true,              // speaker output enabled
  //                              100)) {            // set volume (allowed range: 0(quiet)...127(loud))
  //   halt("Failed to configure speaker output!");
  // }
  // Serial.println("TLV320 DAC config done!");

  Serial.println("Init TLV DAC");
  if (!codec.begin()) {
    halt("Failed to initialize codec!");
  }
  delay(10);

  // Interface Control
  if (!codec.setCodecInterface(TLV320DAC3100_FORMAT_I2S,     // Format: I2S
                               TLV320DAC3100_DATA_LEN_16)) { // Length: 16 bits
    halt("Failed to configure codec interface!");
  }

  // Clock MUX and PLL settings
  if (!codec.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) ||
      !codec.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK)) {
    halt("Failed to configure codec clocks!");
  }

  if (!codec.setPLLValues(1, 2, 32, 0)) { // P=2, R=2, J=32, D=0
    halt("Failed to configure PLL values!");
  }

  // DAC/ADC Config
  if (!codec.setNDAC(true, 8) || // Enable NDAC with value 8
      !codec.setMDAC(true, 2)) { // Enable MDAC with value 2
    halt("Failed to configure DAC dividers!");
  }

  if (!codec.powerPLL(true)) { // Power up the PLL
    halt("Failed to power up PLL!");
  }

  // DAC Setup
  if (!codec.setDACDataPath(true, true,                    // Power up both DACs
                            TLV320_DAC_PATH_NORMAL,        // Normal left path
                            TLV320_DAC_PATH_NORMAL,        // Normal right path
                            TLV320_VOLUME_STEP_1SAMPLE)) { // Step: 1 per sample
    halt("Failed to configure DAC data path!");
  }

  if (!codec.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER, // Left DAC to mixer
                                   TLV320_DAC_ROUTE_MIXER, // Right DAC to mixer
                                   false, false, false,    // No AIN routing
                                   false)) {               // No HPL->HPR
    halt("Failed to configure DAC routing!");
  }

  // DAC Volume Control
  if (!codec.setDACVolumeControl(
          false, false, TLV320_VOL_INDEPENDENT) || // Unmute both channels
      !codec.setChannelVolume(false, 18) ||        // Left DAC +0dB
      !codec.setChannelVolume(true, 18)) {         // Right DAC +0dB
    halt("Failed to configure DAC volumes!");
  }

  // Headphone and Speaker Setup
  if (!codec.configureHeadphoneDriver(
          true, true,                     // Power up both drivers
          TLV320_HP_COMMON_1_35V,         // Default common mode
          false) ||                       // Don't power down on SCD
      !codec.configureHPL_PGA(0, true) || // Set HPL gain, unmute
      !codec.configureHPR_PGA(0, true) || // Set HPR gain, unmute
      !codec.setHPLVolume(true, 6) ||     // Enable and set HPL volume
      !codec.setHPRVolume(true, 6)) {     // Enable and set HPR volume
    halt("Failed to configure headphone outputs!");
  }

  if (!codec.enableSpeaker(true) ||                // Dis/Enable speaker amp
      !codec.configureSPK_PGA(TLV320_SPK_GAIN_6DB, // Set gain to 6dB
                              true) ||             // Unmute
      !codec.setSPKVolume(true, 0)) { // Enable and set volume to 0dB
    halt("Failed to configure speaker output!");
  }

  if (!codec.configMicBias(false, true, TLV320_MICBIAS_AVDD) ||
      !codec.setHeadsetDetect(true) ||
      !codec.setInt1Source(true, true, false, false, false,
                           false) || // GPIO1 is detect headset or button press
      !codec.setGPIO1Mode(TLV320_GPIO1_INT1)) {
    halt("Failed to configure headset detect");
  }
  Serial.println("TLV config done!");
  // // I2S initialization
  i2s.setPins(10, 11, 13);
  if (!i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot)) {
    halt("Failed to initialize I2S!");
  }
  Serial.println("I2S initialization done!");
}

void loop() {
  for (uint32_t i = 0; i < sizeof(audioFile); i += 2) {
    // left channel, low 8 bits first
    i2s.write(audioFile[i]);
    i2s.write(audioFile[i + 1]);
    // right channel, low 8 bits first
    i2s.write(audioFile[i]);
    i2s.write(audioFile[i + 1]);
  }
  //vTaskDelay(1);
    static tlv320_headset_status_t last_status = TLV320_HEADSET_NONE;

  tlv320_headset_status_t status = codec.getHeadsetStatus();

  if (last_status != status) {
    switch (status) {
    case TLV320_HEADSET_NONE:
      Serial.println("Headset removed");
      break;
    case TLV320_HEADSET_WITHOUT_MIC:
      Serial.println("Headphones detected");
      break;
    case TLV320_HEADSET_WITH_MIC:
      Serial.println("Headset with mic detected");
      break;
    }
    last_status = status;
  }

  // Read the sticky IRQ flags
  uint8_t flags = codec.readIRQflags(true);

  // Only print if there are flags set
  if (flags) {
    Serial.println(F("IRQ Flags detected:"));

    if (flags & TLV320DAC3100_IRQ_HPL_SHORT) {
      Serial.println(
          F("- Short circuit detected at HPL / left class-D driver"));
    }

    if (flags & TLV320DAC3100_IRQ_HPR_SHORT) {
      Serial.println(
          F("- Short circuit detected at HPR / right class-D driver"));
    }

    if (flags & TLV320DAC3100_IRQ_BUTTON_PRESS) {
      Serial.println(F("- Headset button pressed"));
    }

    if (flags & TLV320DAC3100_IRQ_HEADSET_DETECT) {
      Serial.println(F("- Headset insertion detected"));
    } else if (flags & 0x10) { // Check bit but with different meaning
      Serial.println(F("- Headset removal detected"));
    }

    if (flags & TLV320DAC3100_IRQ_LEFT_DRC) {
      Serial.println(F("- Left DAC signal power greater than DRC threshold"));
    }

    if (flags & TLV320DAC3100_IRQ_RIGHT_DRC) {
      Serial.println(F("- Right DAC signal power greater than DRC threshold"));
    }

    Serial.print(F("Raw flag value: 0x"));
    Serial.println(flags, HEX);
    Serial.println();
  }

  delay(100);
}