#include "lcd_dual_eyes.h"
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_lvgl_port.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <src/misc/cache/lv_cache.h>
#include <src/draw/sw/lv_draw_sw_utils.h>

#include "gif/lvgl_gif.h"
#include "settings.h"
#include "lvgl_theme.h"
#include "assets/lang_config.h"

#include <algorithm>
#include <font_awesome.h>

#include "board.h"
#include "misc/cache/instance/lv_image_cache.h"


#include <cstring>
#include <vector>

#define TAG "DualSpiLcdDisplay"

DualSpiLcdDisplay::DualSpiLcdDisplay(esp_lcd_panel_io_handle_t io_handle_1, esp_lcd_panel_handle_t panel_handle_1,
                                     esp_lcd_panel_io_handle_t io_handle_2, esp_lcd_panel_handle_t panel_handle_2,
                                     int panel_width, int panel_height, int offset_x, int offset_y,
                                     bool mirror_x, bool mirror_y, bool swap_xy)
    // 父类收到的是第一块面板的 IO 和 panel handle
    : LcdDisplay(io_handle_1, panel_handle_1, panel_width * 2, panel_height) {

    io_handle_2_    = io_handle_2;
    panel_handle_2_ = panel_handle_2;
    panel_width_    = panel_width;
    mirror_x_       = mirror_x;
    mirror_y_       = mirror_y;
    swap_xy_        = swap_xy;

    ESP_LOGI(TAG, "Init dual GC9A01 display %dx%d (single panel %dx%d)",
             panel_width * 2, panel_height, panel_width, panel_height);

    // lvgl_port 只会把方向配置写给第一块面板，所以第二块的面板方向需要我们自己写。
    // (swap_xy/mirror 内部会下发 0x36 MADCTL 命令，所以是带 IO 的，但每个面板各 2 个命令，
    //  整体很快。lvgl_port_add_disp 之后会再对第一块面板写一次 0x36，重复也无副作用。)
    esp_lcd_panel_swap_xy(panel_handle_2, swap_xy);
    esp_lcd_panel_mirror(panel_handle_2, mirror_x, mirror_y);

    // 整屏白屏清屏已经在 gc9a01_display.cc 里对两块面板都做过（按默认方向写 0xFFFF），
    // 全屏 0xFFFF 在新方向下也还是全屏 0xFFFF，没必要再清一次。
    // 千万不要在这里再做"逐行清屏"或"disp_on_off"：
    //   - 两个 panel_io 共享同一根 SPI 总线，每次 DMA 都要抢 SPI 总线互斥量；
    //   - 逐行清屏会产生 240*2*3 = 1440 次 DMA 传输，在共享总线下实测要 10+ 秒，
    //     表现就是"卡在 Init dual GC9A01 display 之后"；

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

#if CONFIG_SPIRAM
    {
        size_t psram_size_mb = esp_psram_get_size() / 1024 / 1024;
        if (psram_size_mb >= 8) {
            lv_image_cache_resize(2 * 1024 * 1024, true);
            ESP_LOGI(TAG, "Use 2MB of PSRAM for image cache");
        } else if (psram_size_mb >= 2) {
            lv_image_cache_resize(512 * 1024, true);
            ESP_LOGI(TAG, "Use 512KB of PSRAM for image cache");
        }
    }
#endif

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    // 让 LVGL 看到的是 2*panel_width x panel_height 的大屏。
    // buffer_size 用 20 行作为分片大小，跟单屏 SpiLcdDisplay 保持一致的比例。
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle     = io_handle_1,
        .panel_handle  = panel_handle_1,
        .control_handle = nullptr,
        .buffer_size   = static_cast<uint32_t>(width_ * 20),
        .double_buffer = false,
        .trans_size    = 0,
        .hres          = static_cast<uint32_t>(width_),
        .vres          = static_cast<uint32_t>(height_),
        .monochrome    = false,
        .rotation      = {
            .swap_xy  = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .flags         = {
            .buff_dma     = 1,
            .buff_spiram  = 0,
            .sw_rotate    = 0,
            .swap_bytes   = 1,
            .full_refresh = 0,
            .direct_mode  = 0,
        },
    };

    ESP_LOGI(TAG, "Adding dual LCD display (%ux%u) via lvgl_port",
             (unsigned)width_, (unsigned)height_);
    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add dual display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // lvgl_port 默认注册的 on_color_trans_done 只在第一块面板 DMA 完成后调 flush_ready，
    // 我们替换成自己的：用 pending_transfers_ 计数，等两块都传输完再调 flush_ready。
    // 必须在持锁状态下原子地替换两个回调（on_color_trans_done 和 flush_cb）：
    //   若替换不原子，LVGL 任务可能在 on_color_trans_done 已是我们的、但 flush_cb 还是
    //   lvgl_port 默认的中间窗口里开始渲染——此时 flush_cb 不会写 pending_transfers_，
    //   io_ready_cb 拿到 0 减 1 后 remaining != 0，永远不会调 lv_disp_flush_ready，
    //   LVGL 任务会卡在等 flush_ready，整个系统死锁。
    pending_transfers_ = 0;
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = &DualSpiLcdDisplay::io_ready_cb,
    };
    lvgl_port_lock(0);
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle_1, &cbs, this));
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle_2, &cbs, this));
    // 用 driver_data 存 this，自定义 flush_cb 里需要拿回实例
    lv_display_set_driver_data(display_, this);
    // 替换 flush_cb 为我们自己的拆分实现
    lv_display_set_flush_cb(display_, &DualSpiLcdDisplay::flush_cb);
    lvgl_port_unlock();

    // 当 LVGL 分辨率变化（例如软件旋转）时，把方向重新同步到第二块面板
    lv_display_add_event_cb(display_, &DualSpiLcdDisplay::on_resolution_changed,
                            LV_EVENT_RESOLUTION_CHANGED, this);

    // 拆分传输用到的中间行缓冲。最大一次可能搬一整行（panel_width 像素）。
    // 用 20 行大小足够覆盖所有 LVGL 分片。
    buf_size_ = panel_width * 20;
    buf_left_  = static_cast<lv_color_t *>(heap_caps_malloc(
        buf_size_ * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    buf_right_ = static_cast<lv_color_t *>(heap_caps_malloc(
        buf_size_ * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (buf_left_ == nullptr || buf_right_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate dual display row buffers");
    }
}

DualSpiLcdDisplay::~DualSpiLcdDisplay() {
    // 防止 io_handle 删除后回调还在被调用（回调持有 this 指针）
    const esp_lcd_panel_io_callbacks_t no_cbs = {
        .on_color_trans_done = nullptr,
    };
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_register_event_callbacks(panel_io_, &no_cbs, nullptr);
    }
    if (io_handle_2_ != nullptr) {
        esp_lcd_panel_io_register_event_callbacks(io_handle_2_, &no_cbs, nullptr);
    }

    // 释放拆分传输用的中间缓冲（基类的析构会处理 display_/panel_/panel_io_）
    if (buf_left_ != nullptr) {
        heap_caps_free(buf_left_);
        buf_left_ = nullptr;
    }
    if (buf_right_ != nullptr) {
        heap_caps_free(buf_right_);
        buf_right_ = nullptr;
    }

    // 释放第二块面板相关资源。第一块由基类析构处理。
    if (panel_handle_2_ != nullptr) {
        esp_lcd_panel_del(panel_handle_2_);
        panel_handle_2_ = nullptr;
    }
    if (io_handle_2_ != nullptr) {
        esp_lcd_panel_io_del(io_handle_2_);
        io_handle_2_ = nullptr;
    }
}

void DualSpiLcdDisplay::on_resolution_changed(lv_event_t *e) {
    auto *self = static_cast<DualSpiLcdDisplay *>(lv_event_get_user_data(e));
    if (self == nullptr || self->panel_handle_2_ == nullptr) {
        return;
    }
    // lvgl_port 内部会更新第一块面板的方向，我们只把第二块同步过去
    esp_lcd_panel_swap_xy(self->panel_handle_2_, self->swap_xy_);
    esp_lcd_panel_mirror(self->panel_handle_2_, self->mirror_x_, self->mirror_y_);
}

void DualSpiLcdDisplay::flush_cb(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map) {
    auto *self = static_cast<DualSpiLcdDisplay *>(lv_display_get_driver_data(drv));
    if (self == nullptr || area == nullptr || color_map == nullptr) {
        lv_disp_flush_ready(drv);
        return;
    }

    const int x1 = area->x1;
    const int y1 = area->y1;
    const int x2 = area->x2;
    const int y2 = area->y2;
    const int half_w = self->panel_width_;
    const int row_w = x2 - x1 + 1;
    const int rows  = y2 - y1 + 1;

    // GC9A01 需要 big-endian RGB565，LVGL 给出的是 host order (小端)，所以交换字节
    if (self->swap_bytes_) {
        lv_draw_sw_rgb565_swap(reinterpret_cast<uint16_t *>(color_map), row_w * rows);
    }

    if (x2 < half_w) {
        // 整块都在左面板：直接把 LVGL 缓冲交给左面板
        taskENTER_CRITICAL(&self->mux_);
        self->pending_transfers_ = 1;
        taskEXIT_CRITICAL(&self->mux_);
        esp_lcd_panel_draw_bitmap(self->panel_, x1, y1, x2 + 1, y2 + 1, color_map);
    } else if (x1 >= half_w) {
        // 整块都在右面板：减去偏移后交给右面板
        taskENTER_CRITICAL(&self->mux_);
        self->pending_transfers_ = 1;
        taskEXIT_CRITICAL(&self->mux_);
        esp_lcd_panel_draw_bitmap(self->panel_handle_2_,
                                  x1 - half_w, y1, x2 + 1 - half_w, y2 + 1,
                                  color_map);
    } else {
        // 渲染区域跨越两块面板：按行拆分到两块面板的中间缓冲
        const int left_w  = half_w - x1;
        const int right_w = x2 - half_w + 1;

        const uint16_t *src = reinterpret_cast<const uint16_t *>(color_map);
        uint16_t *left_buf  = reinterpret_cast<uint16_t *>(self->buf_left_);
        uint16_t *right_buf = reinterpret_cast<uint16_t *>(self->buf_right_);

        for (int y = 0; y < rows; y++) {
            const uint16_t *src_row = src + y * row_w;
            memcpy(left_buf  + y * left_w,  src_row,                left_w  * sizeof(uint16_t));
            memcpy(right_buf + y * right_w, src_row + left_w,       right_w * sizeof(uint16_t));
        }

        taskENTER_CRITICAL(&self->mux_);
        self->pending_transfers_ = 2;
        taskEXIT_CRITICAL(&self->mux_);

        esp_lcd_panel_draw_bitmap(self->panel_, x1, y1, half_w, y2 + 1, self->buf_left_);
        esp_lcd_panel_draw_bitmap(self->panel_handle_2_,
                                  0, y1, right_w, y2 + 1,
                                  self->buf_right_);
    }
    // 注意：这里不调 lv_disp_flush_ready()，等 io_ready_cb 在两块 DMA 都完成后调用
}

bool DualSpiLcdDisplay::io_ready_cb(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    auto *self = static_cast<DualSpiLcdDisplay *>(user_ctx);
    if (self == nullptr) {
        return false;
    }

    int remaining;
    taskENTER_CRITICAL(&self->mux_);
    int old = self->pending_transfers_.load(std::memory_order_relaxed);
    if (old > 0) {
        remaining = self->pending_transfers_.fetch_sub(1, std::memory_order_relaxed) - 1;
    } else {
        // 防御：理论上不应该到这里（flush_cb 一定先 set 再发起 DMA），
        // 但万一发生（例如替换回调的瞬间有正在飞的 DMA），按单次传输处理
        // 调一次 lv_disp_flush_ready，避免 LVGL 卡死。
        remaining = 0;
    }
    taskEXIT_CRITICAL(&self->mux_);

    if (remaining == 0 && self->display_ != nullptr) {
        lv_disp_flush_ready(self->display_);
    }
    return false;
}

void DualSpiLcdDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    ESP_LOGI(TAG, "SetupUI(), eyes only mode");
    Display::SetupUI();
    DisplayLockGuard lock(this);

    lv_display_set_default(display_);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, lvgl_theme->background_color(), 0);

    /* Bottom layer: emoji_box_ - centered display */
    emoji_box_ = lv_obj_create(screen);
    lv_obj_set_size(emoji_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(emoji_box_, 0, 0);
    lv_obj_set_style_border_width(emoji_box_, 0, 0);
    lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, 0);

    /* Eyes GIF for screen1 */
    emoji_image_ = lv_img_create(emoji_box_);
    lv_obj_align(emoji_image_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

    /* Eyes label (fallback icon) for screen1 */
    emoji_label_ = lv_label_create(emoji_box_);
    lv_obj_center(emoji_label_);
    auto large_icon_font = lvgl_theme->large_icon_font()->font();
    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(emoji_label_, FONT_AWESOME_MICROCHIP_AI);

}

// EYES_ONLY mode: stub implementations for required virtual functions
void DualSpiLcdDisplay::SetChatMessage(const char* role, const char* content) {
    // Eyes only mode: chat messages are not displayed
}

void DualSpiLcdDisplay::ClearChatMessages() {
    // Eyes only mode: no chat messages to clear
}

void DualSpiLcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    // Eyes only mode: preview image not used
    (void)image;
}


void DualSpiLcdDisplay::SetEmotionGif(const char* emotion) {

    if (emotion == last_emotion_gif_) {
        return;
    }

    last_emotion_gif_ = emotion;

    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetEmotion('%s') called before SetupUI() - emotion will not be displayed!", emotion);
    }

    if (current_theme_ == nullptr) {
        ESP_LOGW(TAG, "SetEmotion: current_theme_ is nullptr");
        return;
    }
    auto emoji_collection = static_cast<LvglTheme*>(current_theme_)->emoji_collection();
    if (emoji_collection == nullptr) {
        ESP_LOGW(TAG, "SetEmotion: emoji_collection is nullptr");
        return;
    }
    auto image = emoji_collection->GetEmojiImage(emotion);
    if (image == nullptr) {
        ESP_LOGW(TAG, "SetEmotion: GetEmojiImage('%s') returned nullptr", emotion);
        return;
    }

    DisplayLockGuard lock(this);

    // Screen1
    if (image->IsGif()) {
        if (gif_controller_) {
            lv_image_set_src(emoji_image_, nullptr);
            gif_controller_->Stop();
            gif_controller_.reset();
        }
        gif_controller_ = std::make_unique<LvglGif>(image->image_dsc());
        if (gif_controller_->IsLoaded()) {
            gif_controller_->SetFrameCallback([this]() {
                lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            });
            lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            gif_controller_->Start();
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "SetEmotion('%s'): GIF started on screen1", emotion);
        }
    }
    else{
        //ESP_LOGW(TAG, "Screen1 SetEmotion: '%s' is not a GIF", emotion);

        lv_image_set_src(emoji_image_, image->image_dsc());
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }

}

void DualSpiLcdDisplay::SetEmotion(const char* emotion) {
    ESP_LOGI(TAG, "SetEmotion('%s')", emotion);
	if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetEmotion('%s') called before SetupUI() - emotion will not be displayed!", emotion);
    }
#if CONFIG_USE_EYES_ONLY_MESSAGE_STYLE
 
    return;
#endif
}

void DualSpiLcdDisplay::SetTheme(Theme* theme) {
#if CONFIG_USE_EYES_ONLY_MESSAGE_STYLE
    Display::SetTheme(theme);
    ESP_LOGI(TAG, "SetTheme: %s", theme->name().c_str());
    SetEmotionGif("startup");
    return;
#endif
}