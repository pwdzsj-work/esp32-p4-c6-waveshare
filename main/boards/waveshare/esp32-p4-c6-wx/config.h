#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

//#define ISESP32P4_WAVESHARE


// ===== ESP32-P4-WIFI6 + ES8311 + NS4150B PA =====
// This config is matched to the 2026-06-19 13:57 runtime log:
//   codec i2c init ok, SDA=7 SCL=8 addr=0x30

#define AUDIO_INPUT_SAMPLE_RATE   24000
#define AUDIO_OUTPUT_SAMPLE_RATE  24000
#define AUDIO_INPUT_REFERENCE     false
#define AUDIO_CODEC_USE_ES7210    1

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
#define AUDIO_CODEC_ES7210_ADDR   ES7210_CODEC_DEFAULT_ADDR

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
#define AUDIO_CODEC_ES7210_ADDR   ES7210_CODEC_DEFAULT_ADDR

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
#define CAMERA_I2C_FREQ_HZ    100000
#define CAMERA_OV5647_I2C_ADDR_7BIT 0x36
#define CAMERA_MCLK_GPIO      GPIO_NUM_NC   // 新版 CSI 接口无 MCLK/XCLK GPIO
#define CAMERA_XCLK_FREQ_HZ   24000000      // 保留宏；MCLK=NC 时不会输出
#define CAMERA_RESET_GPIO     GPIO_NUM_NC   // 新版原理图未接 CAM_RST 到 ESP32-P4 GPIO
#define CAMERA_PWDN_GPIO      GPIO_NUM_NC   // 新版原理图未接 PWDN 到 ESP32-P4 GPIO

// ===== WS2812B RGB LED =====
// 原理图：DIN -> WS2812B D1 DIN，D1~D8 串联。
// 如果你的主板网名 LED_RGB 对应的 GPIO 不是 GPIO40，把这里改成实际 GPIO。
#define WS2812_LED_GPIO GPIO_NUM_1
#define WS2812_LED_COUNT 8
#define WS2812_RMT_RESOLUTION_HZ (10 * 1000 * 1000)
#define WS2812_LED_BRIGHTNESS 24


// ===== ESP32-P4 <-> ESP32-C6 Wi-Fi6 / BLE Host SDIO =====
// 参考微雪 ESP32-P4-WIFI6 原理图：ESP32-P4 通过 SDIO 连接 ESP32-C6。
// 注意：这些宏只描述硬件连接；真正的 SDIO Host 参数还需要在 config.json/sdkconfig_append 中配置。
#define WIFI_SDIO_SLOT              1
#define WIFI_SDIO_BUS_WIDTH         4
#define WIFI_SDIO_FREQ_KHZ          20000   // 先用 20MHz 稳定启动；稳定后可改 40000

#define WIFI_SDIO_CLK_GPIO          GPIO_NUM_18
#define WIFI_SDIO_CMD_GPIO          GPIO_NUM_19
#define WIFI_SDIO_D0_GPIO           GPIO_NUM_20
#define WIFI_SDIO_D1_GPIO           GPIO_NUM_21
#define WIFI_SDIO_D2_GPIO           GPIO_NUM_16
#define WIFI_SDIO_D3_GPIO           GPIO_NUM_17

// P4 GPIO27 -> C6 CHIP_PU/RESET；CHIP_PU 为高电平运行、低电平复位。
#define WIFI_C6_RESET_GPIO          GPIO_NUM_27
#define WIFI_C6_RESET_ACTIVE_LEVEL  0
#define WIFI_C6_ENABLE_LEVEL        1

// P4 GPIO4 -> WAKE_C6，作为唤醒/保持 C6 工作脚使用。
#define WIFI_C6_WAKE_GPIO           GPIO_NUM_4
#define WIFI_C6_WAKE_ACTIVE_LEVEL   1

// ESP-Hosted SDIO startup can panic inside the driver when the C6 slave does
// not answer enumeration. Keep the P4 side alive by default; set this to 1
// after the C6 SDIO slave firmware and wiring are verified.
#ifndef ESP32_P4_C6_WX_ENABLE_HOSTED_WIFI
#define ESP32_P4_C6_WX_ENABLE_HOSTED_WIFI 1
#endif

#ifndef ESP32_P4_C6_WX_DEFAULT_WIFI_AP_MODE
#define ESP32_P4_C6_WX_DEFAULT_WIFI_AP_MODE 1
#endif

//显示屏大眼睛
#define LCD_SPI_NUM                 SPI2_HOST
#define LCD_SPI_PCLK_HZ             (80*1000*1000)
#define LCD_CMD_BITS                (8)
#define LCD_PARAM_BITS              (8)

#define LCD_SPI_GPIO_CS1            GPIO_NUM_36
#define LCD_SPI_GPIO_CS2            GPIO_NUM_47
#define LCD_SPI_GPIO_DC             GPIO_NUM_10
#define LCD_SPI_GPIO_MOSI           GPIO_NUM_11
#define LCD_SPI_GPIO_CLK            GPIO_NUM_12
#define LCD_SPI_GPIO_RST            GPIO_NUM_37

#define LCD_POWER_GPIO              GPIO_NUM_29
#define LCD_POWER_ACTIVE_LEVEL      0

#define DISPLAY_WIDTH               160
#define DISPLAY_HEIGHT              160
#define DISPLAY_MIRROR_X            false
#define DISPLAY_MIRROR_Y            false
#define DISPLAY_SWAP_XY             false
#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0
#define DISPLAY_BACKLIGHT_PIN                   GPIO_NUM_9
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT         false
#endif  // _BOARD_CONFIG_H_
