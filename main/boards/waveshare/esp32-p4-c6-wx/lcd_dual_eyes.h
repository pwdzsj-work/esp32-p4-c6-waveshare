#ifndef DUAL_SPI_LCD_DISPLAY_H
#define DUAL_SPI_LCD_DISPLAY_H

#include "lcd_display.h"

#include "lvgl_display.h"
#include "gif/lvgl_gif.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>
#include <memory>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <atomic>

// 双 SPI LCD 显示屏
// -----------------------------------------------------------------------------
// 把两块共用同一根 SPI 总线、但 CS 不同的面板（默认水平拼接）拼成一块更大的虚拟
// 显示屏。例如两块 240x240 GC9A01 拼接为 480x240。
//
// LVGL 看到的是 2*panel_width x panel_height，渲染由自定义 flush_cb 按 x 坐标拆
// 成最多两路 DMA 传输分别送给两个面板。两次 DMA 完成后，io_ready_cb 再调
// lv_disp_flush_ready() 通知 LVGL。
class DualSpiLcdDisplay : public LcdDisplay {
public:
    // panel_width / panel_height: 单个物理面板的分辨率
    // 虚拟显示宽度 = 2 * panel_width，高度 = panel_height
    DualSpiLcdDisplay(esp_lcd_panel_io_handle_t io_handle_1, esp_lcd_panel_handle_t panel_handle_1,
                      esp_lcd_panel_io_handle_t io_handle_2, esp_lcd_panel_handle_t panel_handle_2,
                      int panel_width, int panel_height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy);
    virtual ~DualSpiLcdDisplay();

    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override; 
    virtual void ClearChatMessages() override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetPowerSaveMode(bool on) override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void SetupUI() override;
    virtual void SetTheme(Theme* theme) override;

private:
    esp_lcd_panel_io_handle_t io_handle_2_ = nullptr;
    esp_lcd_panel_handle_t    panel_handle_2_ = nullptr;
    int                       panel_width_ = 0;

    // 拆分渲染时用于 DMA 传输的中间行缓冲（每行 panel_width 像素）
    lv_color_t *buf_left_  = nullptr;
    lv_color_t *buf_right_ = nullptr;
    int         buf_size_  = 0; // 像素数

    // 跟踪有多少次 DMA 传输尚未完成（两块的 DMA 回调可能并发触发）
    std::atomic<int> pending_transfers_{0};
    portMUX_TYPE    mux_ = portMUX_INITIALIZER_UNLOCKED;

    bool mirror_x_  = false;
    bool mirror_y_  = false;
    bool swap_xy_   = false;
    bool swap_bytes_ = true;

    const char * last_emotion_gif_ = nullptr;
    void SetEmotionGif(const char* emotion);

    // 把当前 mirror/xy 方向同步到第二块面板（LVGL 分辨率变化时会自动重新应用）
    static void on_resolution_changed(lv_event_t *e);

    static void flush_cb(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map);
    static bool io_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                            esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
};

#endif // DUAL_SPI_LCD_DISPLAY_H
