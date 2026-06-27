#include "spa06_003.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "spa06_003";

#define REG_PRS_B2      0x00
#define REG_TMP_B2      0x03
#define REG_PRS_CFG     0x06
#define REG_TMP_CFG     0x07
#define REG_MEAS_CFG    0x08
#define REG_CFG_REG     0x09
#define REG_RESET       0x0C
#define REG_ID          0x0D
#define REG_COEF        0x10

#define MEAS_PRS_RDY    BIT(4)
#define MEAS_TMP_RDY    BIT(5)
#define MEAS_SENSOR_RDY BIT(6)
#define MEAS_COEF_RDY   BIT(7)

#define CFG_PRS_SHIFT_EN BIT(2)
#define CFG_TMP_SHIFT_EN BIT(3)

static int32_t sign_extend(uint32_t v, uint8_t bits)
{
    uint32_t m = 1U << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

static int32_t read_s24(const uint8_t *b)
{
    return sign_extend(((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2], 24);
}

static uint32_t osr_scale(spa06_osr_t osr)
{
    switch (osr) {
    case SPA06_OSR_1:   return 524288;
    case SPA06_OSR_2:   return 1572864;
    case SPA06_OSR_4:   return 3670016;
    case SPA06_OSR_8:   return 7864320;
    case SPA06_OSR_16:  return 253952;
    case SPA06_OSR_32:  return 516096;
    case SPA06_OSR_64:  return 1040384;
    case SPA06_OSR_128: return 2088960;
    default:            return 524288;
    }
}

static esp_err_t reg_write(spa06_handle_t h, uint8_t reg, uint8_t val)
{
    ESP_RETURN_ON_FALSE(h && h->dev, ESP_ERR_INVALID_ARG, TAG, "bad handle");
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(h->dev, buf, sizeof(buf), 100);
}

static esp_err_t reg_read(spa06_handle_t h, uint8_t reg, uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(h && h->dev && data && len > 0, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    return i2c_master_transmit_receive(h->dev, &reg, 1, data, len, 100);
}

static esp_err_t wait_bits(spa06_handle_t h, uint8_t bits, uint32_t timeout_ms)
{
    uint32_t waited = 0;
    while (waited <= timeout_ms) {
        uint8_t v = 0;
        ESP_RETURN_ON_ERROR(reg_read(h, REG_MEAS_CFG, &v, 1), TAG, "read MEAS_CFG failed");
        if ((v & bits) == bits) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        waited += 5;
    }
    return ESP_ERR_TIMEOUT;
}

static void parse_coefficients(const uint8_t *b, spa06_coef_t *c)
{
    c->c0  = (int16_t)sign_extend(((uint32_t)b[0] << 4) | (b[1] >> 4), 12);
    c->c1  = (int16_t)sign_extend(((uint32_t)(b[1] & 0x0F) << 8) | b[2], 12);
    c->c00 = sign_extend(((uint32_t)b[3] << 12) | ((uint32_t)b[4] << 4) | (b[5] >> 4), 20);
    c->c10 = sign_extend(((uint32_t)(b[5] & 0x0F) << 16) | ((uint32_t)b[6] << 8) | b[7], 20);
    c->c01 = (int16_t)sign_extend(((uint32_t)b[8] << 8) | b[9], 16);
    c->c11 = (int16_t)sign_extend(((uint32_t)b[10] << 8) | b[11], 16);
    c->c20 = (int16_t)sign_extend(((uint32_t)b[12] << 8) | b[13], 16);
    c->c21 = (int16_t)sign_extend(((uint32_t)b[14] << 8) | b[15], 16);
    c->c30 = (int16_t)sign_extend(((uint32_t)b[16] << 8) | b[17], 16);
    c->c31 = (int16_t)sign_extend(((uint32_t)b[18] << 4) | (b[19] >> 4), 12);
    c->c40 = (int16_t)sign_extend(((uint32_t)(b[19] & 0x0F) << 8) | b[20], 12);
}

esp_err_t spa06_create(const spa06_config_t *cfg, spa06_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg && out_handle, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    ESP_RETURN_ON_FALSE(cfg->i2c_addr == SPA06_I2C_ADDR_DEFAULT || cfg->i2c_addr == SPA06_I2C_ADDR_ALT,
                        ESP_ERR_INVALID_ARG, TAG, "bad i2c addr");

    spa06_dev_t *h = calloc(1, sizeof(spa06_dev_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "no mem");

    h->cfg = *cfg;
    if (h->cfg.clk_speed_hz == 0) h->cfg.clk_speed_hz = 400000;
    if (h->cfg.sea_level_hpa <= 0.0f) h->cfg.sea_level_hpa = 1013.25f;

    esp_err_t ret = ESP_OK;
    if (cfg->bus) {
        h->bus = cfg->bus;
        h->own_bus = false;
    } else {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = cfg->port,
            .sda_io_num = cfg->sda_gpio,
            .scl_io_num = cfg->scl_gpio,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true },
        };
        ret = i2c_new_master_bus(&bus_cfg, &h->bus);
        if (ret != ESP_OK) {
            free(h);
            return ret;
        }
        h->own_bus = true;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->i2c_addr,
        .scl_speed_hz = h->cfg.clk_speed_hz,
    };
    ret = i2c_master_bus_add_device(h->bus, &dev_cfg, &h->dev);
    if (ret != ESP_OK) {
        if (h->own_bus && h->bus) i2c_del_master_bus(h->bus);
        free(h);
        return ret;
    }

    h->kp = osr_scale(h->cfg.pressure_osr);
    h->kt = osr_scale(h->cfg.temperature_osr);
    *out_handle = h;
    return ESP_OK;
}

esp_err_t spa06_get_id(spa06_handle_t h, uint8_t *out_id)
{
    ESP_RETURN_ON_FALSE(h && out_id, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    return reg_read(h, REG_ID, out_id, 1);
}

esp_err_t spa06_init(spa06_handle_t h)
{
    ESP_RETURN_ON_FALSE(h && h->dev, ESP_ERR_INVALID_ARG, TAG, "bad handle");

    // Soft reset
    ESP_RETURN_ON_ERROR(reg_write(h, REG_RESET, 0x09), TAG, "reset failed");
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_RETURN_ON_ERROR(wait_bits(h, MEAS_SENSOR_RDY | MEAS_COEF_RDY, 100), TAG, "sensor/coef not ready");

    uint8_t id = 0;
    esp_err_t id_ret = spa06_get_id(h, &id);
    if (id_ret == ESP_OK) {
        ESP_LOGI(TAG, "ID=0x%02X addr=0x%02X", id, h->cfg.i2c_addr);
    }

    uint8_t coef_raw[21] = {0};
    ESP_RETURN_ON_ERROR(reg_read(h, REG_COEF, coef_raw, sizeof(coef_raw)), TAG, "read coef failed");
    parse_coefficients(coef_raw, &h->coef);

    uint8_t prs_cfg = ((uint8_t)h->cfg.pressure_rate << 4) | ((uint8_t)h->cfg.pressure_osr & 0x0F);
    uint8_t tmp_cfg = ((uint8_t)h->cfg.temperature_rate << 4) | ((uint8_t)h->cfg.temperature_osr & 0x0F);
    ESP_RETURN_ON_ERROR(reg_write(h, REG_PRS_CFG, prs_cfg), TAG, "write PRS_CFG failed");
    ESP_RETURN_ON_ERROR(reg_write(h, REG_TMP_CFG, tmp_cfg), TAG, "write TMP_CFG failed");

    uint8_t cfg_reg = 0;
    if (h->cfg.pressure_osr >= SPA06_OSR_16) cfg_reg |= CFG_PRS_SHIFT_EN;
    if (h->cfg.temperature_osr >= SPA06_OSR_16) cfg_reg |= CFG_TMP_SHIFT_EN;
    ESP_RETURN_ON_ERROR(reg_write(h, REG_CFG_REG, cfg_reg), TAG, "write CFG_REG failed");

    // 0x07 = background continuous pressure + temperature
    ESP_RETURN_ON_ERROR(reg_write(h, REG_MEAS_CFG, 0x07), TAG, "start background P+T failed");
    vTaskDelay(pdMS_TO_TICKS(80));
    return ESP_OK;
}

esp_err_t spa06_read(spa06_handle_t h, spa06_data_t *out)
{
    ESP_RETURN_ON_FALSE(h && out, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    ESP_RETURN_ON_ERROR(wait_bits(h, MEAS_PRS_RDY | MEAS_TMP_RDY, 300), TAG, "data not ready");

    uint8_t raw[6] = {0};
    ESP_RETURN_ON_ERROR(reg_read(h, REG_PRS_B2, raw, sizeof(raw)), TAG, "read raw P/T failed");

    int32_t p_raw = read_s24(&raw[0]);
    int32_t t_raw = read_s24(&raw[3]);

    const double p_sc = (double)p_raw / (double)h->kp;
    const double t_sc = (double)t_raw / (double)h->kt;
    const spa06_coef_t *c = &h->coef;

    double p_pa = c->c00
        + c->c10 * p_sc
        + c->c20 * p_sc * p_sc
        + c->c30 * p_sc * p_sc * p_sc
        + c->c40 * p_sc * p_sc * p_sc * p_sc
        + t_sc * (c->c01 + c->c11 * p_sc + c->c21 * p_sc * p_sc + c->c31 * p_sc * p_sc * p_sc);

    double t_c = c->c0 * 0.5 + c->c1 * t_sc;

    out->pressure_pa = (float)p_pa;
    out->pressure_hpa = (float)(p_pa / 100.0);
    out->temperature_c = (float)t_c;
    out->altitude_m = 44330.0f * (1.0f - powf(out->pressure_hpa / h->cfg.sea_level_hpa, 1.0f / 5.255f));

    return ESP_OK;
}

esp_err_t spa06_destroy(spa06_handle_t h)
{
    if (!h) return ESP_OK;
    if (h->dev) {
        i2c_master_bus_rm_device(h->dev);
    }
    if (h->own_bus && h->bus) {
        i2c_del_master_bus(h->bus);
    }
    free(h);
    return ESP_OK;
}
