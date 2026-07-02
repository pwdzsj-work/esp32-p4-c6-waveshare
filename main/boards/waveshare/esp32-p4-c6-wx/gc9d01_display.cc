#include "gc9d01_display.h"
#include "config.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9d01.h>
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "GC9d01Display";

static const gc9d01_lcd_init_cmd_t vendor_specific_init_gc9d01[] = {
    {0xFE, (uint8_t[]){0x00}, 0, 0},
    {0xEF, (uint8_t[]){0x00}, 0, 0},
    {0x80, (uint8_t[]){0xFF}, 1, 0},
    {0x81, (uint8_t[]){0xFF}, 1, 0},
    {0x82, (uint8_t[]){0xFF}, 1, 0},
    {0x83, (uint8_t[]){0xFF}, 1, 0},
    {0x84, (uint8_t[]){0xFF}, 1, 0},
    {0x85, (uint8_t[]){0xFF}, 1, 0},
    {0x86, (uint8_t[]){0xFF}, 1, 0},
    {0x87, (uint8_t[]){0xFF}, 1, 0},
    {0x88, (uint8_t[]){0xFF}, 1, 0},
    {0x89, (uint8_t[]){0xFF}, 1, 0},
    {0x8A, (uint8_t[]){0xFF}, 1, 0},
    {0x8B, (uint8_t[]){0xFF}, 1, 0},
    {0x8C, (uint8_t[]){0xFF}, 1, 0},
    {0x8D, (uint8_t[]){0xFF}, 1, 0},
    {0x8E, (uint8_t[]){0xFF}, 1, 0},
    {0x8F, (uint8_t[]){0xFF}, 1, 0},
    {0x3A, (uint8_t[]){0x05}, 1, 0},
    {0xEC, (uint8_t[]){0x01}, 1, 0},
    {0x74, (uint8_t[]){0x02, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x98, (uint8_t[]){0x3E}, 1, 0},
    {0x99, (uint8_t[]){0x3E}, 1, 0},
    {0xB5, (uint8_t[]){0x0D, 0x0D}, 2, 0},
    {0x60, (uint8_t[]){0x38, 0x0F, 0x79, 0x67}, 4, 0},
    {0x61, (uint8_t[]){0x38, 0x11, 0x79, 0x67}, 4, 0},
    {0x64, (uint8_t[]){0x38, 0x17, 0x71, 0x5F, 0x79, 0x67}, 6, 0},
    {0x65, (uint8_t[]){0x38, 0x13, 0x71, 0x5B, 0x79, 0x67}, 6, 0},
    {0x6A, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x6C, (uint8_t[]){0x22, 0x02, 0x22, 0x02, 0x22, 0x22, 0x50}, 7, 0},
    {0x6E, (uint8_t[]){0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0x0F, 0x0F, 0x0D, 0x0D, 0x0B, 0x0B, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x0A, 0x0C, 0x0C, 0x0E, 0x0E, 0x10, 0x10, 0x00, 0x00, 0x02, 0x02, 0x04, 0x04}, 32, 0},
    {0xBF, (uint8_t[]){0x01}, 1, 0},
    {0xF9, (uint8_t[]){0x40}, 1, 0},
    {0x9B, (uint8_t[]){0x3B}, 1, 0},
    {0x93, (uint8_t[]){0x33, 0x7F, 0x00}, 3, 0},
    {0x7E, (uint8_t[]){0x30}, 1, 0},
    {0x70, (uint8_t[]){0x0D, 0x02, 0x08, 0x0D, 0x02, 0x08}, 6, 0},
    {0x71, (uint8_t[]){0x0D, 0x02, 0x08}, 3, 0},
    {0x91, (uint8_t[]){0x0E, 0x09}, 2, 0},
    {0xC3, (uint8_t[]){0x18}, 1, 0},
    {0xC4, (uint8_t[]){0x18}, 1, 0},
    {0xC9, (uint8_t[]){0x3C}, 1, 0},

    {0xF0, (uint8_t []){0x13, 0x15, 0x04, 0x05, 0x01, 0x38}, 6, 0},
    {0xF2, (uint8_t []){0x13, 0x15, 0x04, 0x05, 0x01, 0x34}, 6, 0},
    {0xF1, (uint8_t []){0x4B, 0xB8, 0x7B, 0x34, 0x35, 0xEF}, 6, 0},
    {0xF3, (uint8_t []){0x47, 0xB4, 0x72, 0x34, 0x35, 0xDA}, 6, 0},
    //{0x36, (uint8_t []){0xc0}, 1, 0},
    {0x36, (uint8_t []){0x00},1,0},
        //D7 (0x80): MY (Mirror Y) - 上下翻转
        //D6 (0x40): MX (Mirror X) - 左右翻转
        //D5 (0x20): MV (Exchange XY) - 行列交换（横屏/竖屏切换）
        //D3 (0x08): RGB/BGR 顺序（如果颜色红蓝反了，就修改这一位）

    {0xB4, (uint8_t []){0x00, 0x00}, 2, 0},
    {0x34, (uint8_t []){0x00}, 0, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 1, 0},   //打开显示
};

Gc9d01DisplayHandles InitializeGc9d01Display() {
    Gc9d01DisplayHandles handles = {nullptr, nullptr, nullptr, nullptr};

    ESP_LOGI(TAG, "Init GC9d01 display");

    if (LCD_POWER_GPIO != GPIO_NUM_NC) {
        gpio_config_t lcd_power_config = {
            .pin_bit_mask = (BIT64(LCD_POWER_GPIO)),
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&lcd_power_config));
        gpio_set_level(LCD_POWER_GPIO, 1);
    }

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle_1 = NULL;
    esp_lcd_panel_io_handle_t io_handle_2 = NULL;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = LCD_SPI_GPIO_CS1;
    io_config.dc_gpio_num = LCD_SPI_GPIO_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = LCD_SPI_PCLK_HZ;
    io_config.trans_queue_depth = 1;
    io_config.lcd_cmd_bits = LCD_CMD_BITS;
    io_config.lcd_param_bits = LCD_PARAM_BITS;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_NUM, &io_config, &io_handle_1));
    io_config.cs_gpio_num = LCD_SPI_GPIO_CS2;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_NUM, &io_config, &io_handle_2));

    ESP_LOGI(TAG, "Install GC9d01 panel driver");
    esp_lcd_panel_handle_t panel_handle_1 = NULL;
    esp_lcd_panel_handle_t panel_handle_2 = NULL;

    gc9d01_vendor_config_t gc9d01_vendor_config = {
        .init_cmds = vendor_specific_init_gc9d01,
        .init_cmds_size = sizeof(vendor_specific_init_gc9d01) / sizeof(gc9d01_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = LCD_SPI_GPIO_RST;
    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = &gc9d01_vendor_config;
    panel_config.flags.reset_active_high = false;

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9d01(io_handle_1, &panel_config, &panel_handle_1));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_1));

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9d01(io_handle_2, &panel_config, &panel_handle_2));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_2));

    uint8_t refresh_rate_cmd = 0xB1;
    uint8_t refresh_rate_params[] = {0x01, 0x1A, 0x1B};

    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle_1, refresh_rate_cmd, refresh_rate_params, sizeof(refresh_rate_params)));
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle_2, refresh_rate_cmd, refresh_rate_params, sizeof(refresh_rate_params)));

    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_1));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle_1, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_1, true));

    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_2));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle_2, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_2, true));

    std::vector<uint16_t> buffer(DISPLAY_WIDTH * DISPLAY_HEIGHT, 0xFFFF);
    esp_lcd_panel_draw_bitmap(panel_handle_1, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, buffer.data());
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_lcd_panel_draw_bitmap(panel_handle_2, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, buffer.data());

    handles.io_handle_1 = io_handle_1;
    handles.io_handle_2 = io_handle_2;
    handles.panel_handle_1 = panel_handle_1;
    handles.panel_handle_2 = panel_handle_2;

    return handles;
}
