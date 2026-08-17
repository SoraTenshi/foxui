#pragma once

#include "foxui_platform.h"

struct Foxui_Draw_List;
struct Foxui_Window;
using Foxui_Render_Frame_Fn = void(*)(Foxui_Window *window);

struct Foxui_Window_Flags {
    bool should_close       : 1;
    bool native_decorations : 1;
    bool round_corners      : 1;
    bool is_sizing          : 1;
    u32  reserved_          : 4;
};

struct Foxui_Window {
    void                 *native_window;
    Foxui_Render_Frame_Fn render_frame;
    u32                   dpi;
    Foxui_Rect            titlebar_rect;
    Foxui_Rect            content_rect;
    Foxui_Rect            client_rect;
    Foxui_Window_Flags    flags;
    void                 *user_data;
};

struct Foxui_Window_Description {
    String8 title;
    s32     width;
    s32     height;
};

struct Foxui_Vertex {
    Foxui_Point point;
    Foxui_Color color;
};

struct Foxui_Draw_Command {
    Foxui_Rect clip_rect;
    u32        first_index;
    u32        index_count;
};

struct Foxui_Draw_List {
    Foxui_Vertex *vertices;
    u32           vertex_count;
    u32           vertex_capacity;
    
    u32 *indices;
    u32  index_count;
    u32  index_capacity;
    
    Foxui_Draw_Command *commands;
    u32                 command_count;
    u32                 command_capacity;
};

bool foxui_create_window(
    Foxui_Window             *window,
    Foxui_Window_Description  description,
    Foxui_Render_Frame_Fn     render_frame,
    void                     *user_data
);
void foxui_destroy_window(Foxui_Window *window);

void foxui_begin_draw_command(Foxui_Window *window, Foxui_Draw_List *list, Foxui_Rect rect);
void foxui_end_draw_command(Foxui_Window *window, Foxui_Draw_List *list);

void foxui_begin_content(Foxui_Window *window, Foxui_Draw_List *list);
void foxui_end_content(Foxui_Window *window, Foxui_Draw_List *list);

void foxui_begin_frame(Foxui_Window *window, Foxui_Draw_List *list);
void foxui_end_frame(Foxui_Window *window, Foxui_Draw_List *list);
void foxui_present(Foxui_Window *window);

void foxui_begin_titlebar(Foxui_Window *window, Foxui_Draw_List *list);
void foxui_end_titlebar(Foxui_Window *window, Foxui_Draw_List *list);

void foxui_spinning_triangle_titlebar(Foxui_Window *window, Foxui_Draw_List *list, f32 time);
void foxui_spinning_triangle_client(Foxui_Window *window, Foxui_Draw_List *list, f32 time);

bool foxui_poll_events(Foxui_Window *window);
bool foxui_wait_events(Foxui_Window *window);
