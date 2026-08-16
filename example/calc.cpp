#include "foxui.h"

#include <windows.h>

static void render_frame_fn(Foxui_Window *window, void *) {
    foxui_begin_frame(window);
    f32 time = (f32)GetTickCount64() / 1000.f;
    foxui_spinning_triangle_client(window, time);
    foxui_spinning_triangle_titlebar(window, time);
    
    foxui_draw_titlebar(window);
    foxui_end_frame(window);
}

int main(void) {
    Foxui_Window window = {};
    if(!foxui_create_window(&window, {S8("Foxui Calculator"), 960, 540}, &render_frame_fn)) {
        return 1;
    }
    
    while(foxui_poll_events(&window)) {
        window.render_frame(&window, nullptr);
    }
    
    foxui_destroy_window(&window);
    return 0;
}