#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ===== ESP32-P4-WIFI6 + ES8311 + NS4150B PA =====
// This config is matched to the 2026-06-19 13:57 runtime log:
//   codec i2c init ok, SDA=7 SCL=8 addr=0x30

#define AUDIO_INPUT_SAMPLE_RATE   24000
#define AUDIO_OUTPUT_SAMPLE_RATE  24000

#define ISESP32P4_WAVESHARE

#ifdef ISESP32P4_WAVESHARE

// ESP32-P4-WIFI6 schematic mapping:
// ES8311: ASDOUT->GPIO11, LRCK->GPIO10, DSDIN->GPIO9, SCLK->GPIO12, MCLK->GPIO13
#define AUDIO_I2S_GPIO_MCLK       GPIO_NUM_13
#define AUDIO_I2S_GPIO_BCLK       GPIO_NUM_12
#define AUDIO_I2S_GPIO_WS         GPIO_NUM_10
#define AUDIO_I2S_GPIO_DOUT       GPIO_NUM_9
#define AUDIO_I2S_GPIO_DIN        GPIO_NUM_11

// ES8311 I2C. xiaozhi Es8311AudioCodec uses the 8-bit address form here.
#define AUDIO_CODEC_I2C_SDA_PIN   GPIO_NUM_7
#define AUDIO_CODEC_I2C_SCL_PIN   GPIO_NUM_8
#define AUDIO_CODEC_ES8311_ADDR   ES8311_CODEC_DEFAULT_ADDR

// NS4150B PA_CTRL: schematic shows GPIO53 -> R68 0R -> PA_CTRL.
#define AUDIO_CODEC_PA_PIN        GPIO_NUM_53
#define AUDIO_CODEC_PA_INVERTED   false

#else

// ESP32-P4-WIFI6 schematic mapping:
#define AUDIO_I2S_GPIO_MCLK       GPIO_NUM_53
#define AUDIO_I2S_GPIO_BCLK       GPIO_NUM_52
#define AUDIO_I2S_GPIO_WS         GPIO_NUM_48
#define AUDIO_I2S_GPIO_DOUT       GPIO_NUM_54
#define AUDIO_I2S_GPIO_DIN        GPIO_NUM_NC   //录音脚

// ES8311 I2C. xiaozhi Es8311AudioCodec uses the 8-bit address form here.
#define AUDIO_CODEC_I2C_SDA_PIN   GPIO_NUM_15
#define AUDIO_CODEC_I2C_SCL_PIN   GPIO_NUM_14
#define AUDIO_CODEC_ES8311_ADDR   ES8311_CODEC_DEFAULT_ADDR

// NS4150B PA_CTRL: schematic shows GPIO53 -> R68 0R -> PA_CTRL.
#define AUDIO_CODEC_PA_PIN        GPIO_NUM_43
#define AUDIO_CODEC_PA_INVERTED   false
#endif


#endif  // _BOARD_CONFIG_H_
