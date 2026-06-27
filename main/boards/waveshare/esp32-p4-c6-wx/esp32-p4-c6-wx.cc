#include "wifi_board.h"
#include "audio/codecs/es8311_audio_codec.h"
#include "config.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_video.h"
#include "esp_video_init.h"

#include "spa06_003.h"

static const char* TAG = "ESP32_P4_C6_WX";

static spa06_handle_t g_spa06 = nullptr;

static void Spa06Task(void*) {
    while (true) {
        spa06_data_t data = {};
        if (g_spa06 != nullptr) {
            esp_err_t ret = spa06_read(g_spa06, &data);
            if (ret == ESP_OK) {
                ESP_LOGI("SPA06", "pressure=%.2f hPa temp=%.2f C altitude=%.1f m",
                         data.pressure_hpa, data.temperature_c, data.altitude_m);
            } else {
                ESP_LOGW("SPA06", "read failed: %s", esp_err_to_name(ret));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


class Esp32P4C6WxBoard : public WifiBoard {
private:
    EspVideo* camera_ = nullptr;
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

    void InitializeSpa06() {
        if (g_spa06 != nullptr) {
            ESP_LOGW(TAG, "InitializeSpa06() already called, skip");
            return;
        }

        if (codec_i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "SPA06 init skipped: shared I2C bus is null");
            return;
        }

        spa06_config_t cfg = {};
        cfg.bus = codec_i2c_bus_;              // 复用 ES8311/Camera 已创建的 I2C0，避免 I2C bus already acquired
        cfg.port = I2C_NUM_0;
        cfg.sda_gpio = AUDIO_CODEC_I2C_SDA_PIN;
        cfg.scl_gpio = AUDIO_CODEC_I2C_SCL_PIN;
        cfg.clk_speed_hz = 400000;
        cfg.i2c_addr = SPA06_I2C_ADDR_DEFAULT; // SDO 悬空/上拉=0x77；SDO 下拉=0x76
        cfg.pressure_rate = SPA06_RATE_4_HZ;
        cfg.temperature_rate = SPA06_RATE_4_HZ;
        cfg.pressure_osr = SPA06_OSR_16;
        cfg.temperature_osr = SPA06_OSR_16;
        cfg.sea_level_hpa = 1013.25f;

        esp_err_t ret = i2c_master_probe(codec_i2c_bus_, cfg.i2c_addr, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SPA06 I2C probe OK, addr7=0x%02X", cfg.i2c_addr);
        } else {
            ESP_LOGW(TAG, "SPA06 I2C probe failed: %s, addr7=0x%02X", esp_err_to_name(ret), cfg.i2c_addr);
        }

        ESP_ERROR_CHECK(spa06_create(&cfg, &g_spa06));
        ESP_ERROR_CHECK(spa06_init(g_spa06));
        xTaskCreate(Spa06Task, "spa06", 4096, nullptr, 5, nullptr);

        ESP_LOGI(TAG, "SPA06 pressure sensor initialized");
    }

    void InitializeCamera() {
        if (camera_ != nullptr) {
            ESP_LOGW(TAG, "InitializeCamera() already called, skip");
            return;
        }

        if (codec_i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "Camera init skipped: shared I2C bus is null");
            return;
        }

        ESP_LOGI(TAG, "Camera photo enabled: OV5647 + MIPI CSI + shared I2C");
        ESP_LOGI(TAG, "Camera reuses shared ESP_I2C bus, port=%d SDA=%d SCL=%d",
                 CAMERA_I2C_PORT, CAMERA_I2C_SDA_PIN, CAMERA_I2C_SCL_PIN);

        // OV5647 常用 SCCB/I2C 7-bit 地址是 0x36。探测失败也继续创建 EspVideo，方便驱动给出更完整日志。
        esp_err_t ret = i2c_master_probe(codec_i2c_bus_, CAMERA_OV5647_I2C_ADDR_7BIT, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OV5647 I2C probe OK, addr7=0x%02X", CAMERA_OV5647_I2C_ADDR_7BIT);
        } else {
            ESP_LOGW(TAG, "OV5647 I2C probe failed: %s, addr7=0x%02X", esp_err_to_name(ret), CAMERA_OV5647_I2C_ADDR_7BIT);
        }

        esp_video_init_csi_config_t csi_config = {};
        csi_config.sccb_config.init_sccb = false;        // 复用已经初始化好的 codec_i2c_bus_
        csi_config.sccb_config.i2c_handle = codec_i2c_bus_;
        csi_config.sccb_config.freq = CAMERA_I2C_FREQ_HZ;
        csi_config.reset_pin = CAMERA_RESET_GPIO;
        csi_config.pwdn_pin = CAMERA_PWDN_GPIO;

        esp_video_init_config_t camera_config = {};
        camera_config.csi = &csi_config;

        ESP_LOGI(TAG, "Camera config will pass to EspVideo: shared_i2c=%p RST=%d PWDN=%d SDA=%d SCL=%d",
                 codec_i2c_bus_, CAMERA_RESET_GPIO, CAMERA_PWDN_GPIO, CAMERA_I2C_SDA_PIN, CAMERA_I2C_SCL_PIN);

        // 不要在这里手动调用 esp_video_init()。
        // EspVideo 构造函数内部会调用一次 esp_video_init(camera_config)。
        // 如果这里先调一次，EspVideo 内部再调第二次，会导致 ISP 设备重复注册：
        //   video name=ISP id=20 has been registered
        //   esp_video_init: failed to create hardware ISP video device

        // 创建成功后，小智框架会在 MCP 中注册 self.camera.take_photo，用语音或 MCP 调用拍照。
        camera_ = new EspVideo(camera_config);

        ESP_LOGI(TAG, "Camera object created; GetCamera() will expose self.camera.take_photo");
    }

public:
        Esp32P4C6WxBoard() {
        InitializeCodecI2c();
        InitializeSpa06();
        InitializeCamera();
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

        virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(Esp32P4C6WxBoard);
