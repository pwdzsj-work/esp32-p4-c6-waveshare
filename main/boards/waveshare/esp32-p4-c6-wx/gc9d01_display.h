#ifndef _GC9D01_DISPLAY_H_
#define _GC9D01_DISPLAY_H_

#include <stdint.h>
#include <vector>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

struct Gc9d01DisplayHandles {
    esp_lcd_panel_io_handle_t io_handle_1;
    esp_lcd_panel_io_handle_t io_handle_2;
    esp_lcd_panel_handle_t panel_handle_1;
    esp_lcd_panel_handle_t panel_handle_2;
};

Gc9d01DisplayHandles InitializeGc9d01Display();

#endif
