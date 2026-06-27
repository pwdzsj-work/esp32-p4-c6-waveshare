#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define ISESP32P4_WAVESHARE


// ===== ESP32-P4-WIFI6 + ES8311 + NS4150B PA =====
// This config is matched to the 2026-06-19 13:57 runtime log:
//   codec i2c init ok, SDA=7 SCL=8 addr=0x30

#define AUDIO_INPUT_SAMPLE_RATE   24000
#define AUDIO_OUTPUT_SAMPLE_RATE  24000



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

// ===== Camera: MIPI CSI + SCCB/I2C =====
// 摄像头为 OV5647，走 ESP32-P4 MIPI CSI 专用差分信号；CSI D0/D1/CLK 不按普通 GPIO 配。
// 摄像头 SCCB/I2C 与 ES8311 共用 ESP_I2C 总线：SDA=GPIO7，SCL=GPIO8。
// 注意：必须复用 codec_i2c_bus_，不能再新开 I2C_NUM_1，否则容易和 ES8311 总线冲突。
#define CAMERA_I2C_PORT       I2C_NUM_0
#define CAMERA_I2C_SDA_PIN    GPIO_NUM_7    // J1/J4 ESP_I2C_SDA，与 ES8311 SDA 共线
#define CAMERA_I2C_SCL_PIN    GPIO_NUM_8    // J1/J4 ESP_I2C_SCL，与 ES8311 SCL 共线
#define CAMERA_I2C_FREQ_HZ    400000
#define CAMERA_OV5647_I2C_ADDR_7BIT 0x36
#define CAMERA_MCLK_GPIO      GPIO_NUM_NC   // 新版 CSI 接口无 MCLK/XCLK GPIO
#define CAMERA_XCLK_FREQ_HZ   24000000      // 保留宏；MCLK=NC 时不会输出
#define CAMERA_RESET_GPIO     GPIO_NUM_NC   // 新版原理图未接 CAM_RST 到 ESP32-P4 GPIO
#define CAMERA_PWDN_GPIO      GPIO_NUM_NC   // 新版原理图未接 PWDN 到 ESP32-P4 GPIO

#endif  // _BOARD_CONFIG_H_
