#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPA06_I2C_ADDR_DEFAULT   0x77
#define SPA06_I2C_ADDR_ALT       0x76

typedef enum {
    SPA06_RATE_1_HZ   = 0,
    SPA06_RATE_2_HZ   = 1,
    SPA06_RATE_4_HZ   = 2,
    SPA06_RATE_8_HZ   = 3,
    SPA06_RATE_16_HZ  = 4,
    SPA06_RATE_32_HZ  = 5,
    SPA06_RATE_64_HZ  = 6,
    SPA06_RATE_128_HZ = 7,
} spa06_rate_t;

typedef enum {
    SPA06_OSR_1   = 0,
    SPA06_OSR_2   = 1,
    SPA06_OSR_4   = 2,
    SPA06_OSR_8   = 3,
    SPA06_OSR_16  = 4,
    SPA06_OSR_32  = 5,
    SPA06_OSR_64  = 6,
    SPA06_OSR_128 = 7,
} spa06_osr_t;

typedef struct {
    // 如果小智板级已经创建了 I2C bus，这里传已有 bus；否则传 NULL，驱动会自己创建。
    i2c_master_bus_handle_t bus;
    i2c_port_t port;
    int sda_gpio;
    int scl_gpio;
    uint32_t clk_speed_hz;
    uint8_t i2c_addr;

    spa06_rate_t pressure_rate;
    spa06_rate_t temperature_rate;
    spa06_osr_t pressure_osr;
    spa06_osr_t temperature_osr;

    float sea_level_hpa;
} spa06_config_t;

typedef struct {
    float pressure_pa;
    float pressure_hpa;
    float temperature_c;
    float altitude_m;
} spa06_data_t;

typedef struct {
    int16_t c0;
    int16_t c1;
    int32_t c00;
    int32_t c10;
    int16_t c01;
    int16_t c11;
    int16_t c20;
    int16_t c21;
    int16_t c30;
    int16_t c31;
    int16_t c40;
} spa06_coef_t;

typedef struct spa06_dev {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    bool own_bus;
    spa06_config_t cfg;
    spa06_coef_t coef;
    uint32_t kp;
    uint32_t kt;
} spa06_dev_t;

typedef spa06_dev_t *spa06_handle_t;

esp_err_t spa06_create(const spa06_config_t *cfg, spa06_handle_t *out_handle);
esp_err_t spa06_init(spa06_handle_t h);
esp_err_t spa06_read(spa06_handle_t h, spa06_data_t *out);
esp_err_t spa06_get_id(spa06_handle_t h, uint8_t *out_id);
esp_err_t spa06_destroy(spa06_handle_t h);

#ifdef __cplusplus
}
#endif
