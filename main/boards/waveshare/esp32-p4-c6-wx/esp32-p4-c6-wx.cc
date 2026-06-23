#include "wifi_board.h"
#include "audio/codecs/es8311_audio_codec.h"
#include "config.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

static const char* TAG = "ESP32_P4_C6_WX";

class Esp32P4C6WxBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.flags.enable_internal_pullup = true;

        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

        // i2c_master_probe 使用 7-bit 地址；ES8311 0x30 是 8-bit 地址，所以右移 1 位变 0x18。
        esp_err_t ret = i2c_master_probe(codec_i2c_bus_, AUDIO_CODEC_ES8311_ADDR >> 1, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "ES8311 I2C OK, SDA=%d SCL=%d addr8=0x%02X addr7=0x%02X",
                     AUDIO_CODEC_I2C_SDA_PIN,
                     AUDIO_CODEC_I2C_SCL_PIN,
                     AUDIO_CODEC_ES8311_ADDR,
                     AUDIO_CODEC_ES8311_ADDR >> 1);
        } else {
            ESP_LOGW(TAG, "ES8311 I2C probe failed: %s, SDA=%d SCL=%d addr8=0x%02X addr7=0x%02X",
                     esp_err_to_name(ret),
                     AUDIO_CODEC_I2C_SDA_PIN,
                     AUDIO_CODEC_I2C_SCL_PIN,
                     AUDIO_CODEC_ES8311_ADDR,
                     AUDIO_CODEC_ES8311_ADDR >> 1);
            // 不在这里中断，后面 Es8311AudioCodec 初始化还会给出更完整错误。
        }
    }

public:
    Esp32P4C6WxBoard() {
        InitializeCodecI2c();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }
};

DECLARE_BOARD(Esp32P4C6WxBoard);
