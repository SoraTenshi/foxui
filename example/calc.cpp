#include "foxui.h"

#include <windows.h>

struct App_State {
    Foxui_Vertex vertices[4096];
    u32 indices[8192];
    
    Foxui_Draw_List list;
};

static void render_frame_fn(Foxui_Window *window) {
    App_State *app = (App_State *)window->user_data;
    foxui_begin_frame(window, &app->list);
    f32 time = (f32)GetTickCount64() / 1000.f;
    foxui_spinning_triangle_client(window, &app->list, time);
    
    foxui_draw_titlebar(window, &app->list);
    foxui_spinning_triangle_titlebar(window, &app->list, time);
    foxui_end_frame(window, &app->list);
}

int main(void) {
    App_State state = {};
    state.list.vertices = state.vertices;
    state.list.vertex_capacity = FOXUI_ARRAY_COUNT(state.vertices);
    state.list.indices = state.indices;
    state.list.index_capacity = FOXUI_ARRAY_COUNT(state.indices);

    Foxui_Window window = {};
    if(!foxui_create_window(
        &window,
        {S8("Foxui Calculator"), 960, 540},
        &render_frame_fn,
        &state
    )) {
        return 1;
    }
    
    while(foxui_poll_events(&window)) {
        window.render_frame(&window);
    }
    
    foxui_destroy_window(&window);
    return 0;
}