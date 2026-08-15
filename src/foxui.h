#pragma once

#include "foxui_platform.h"

struct Foxui_Titlebar_Rect {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
};

struct Foxui_Window_Flags {
    bool should_close       : 1;
    bool native_decorations : 1;
    bool round_corners      : 1;
    u32  reserved_          : 5;
};

struct Foxui_Window {
    void               *native_window;
    u32                 dpi;
    Foxui_Titlebar_Rect titlebar_rect;
    Foxui_Window_Flags  flags;
};

struct Foxui_Window_Description {
    String8 title;
    s32     width;
    s32     height;
};

bool foxui_create_window(
    Foxui_Window             *window,
    Foxui_Window_Description  description
);
void foxui_destroy_window(Foxui_Window *window);

bool foxui_poll_events(Foxui_Window *window);
bool foxui_wait_events(Foxui_Window *window);
