#include "foxui.h"

#include <math.h>

FOXUI_INTERNAL Foxui_Vertex foxui_vertex(f32 x, f32 y, Foxui_Color color) {
    return Foxui_Vertex {
        .point = {x, y},
        .color = color,
    };
}

FOXUI_INTERNAL void foxui_draw_list_reset(Foxui_Draw_List *list) {
    list->vertex_count = 0;
    list->index_count = 0;
    list->command_count = 0;
}

FOXUI_INTERNAL bool foxui_draw_list_can_push(
    Foxui_Draw_List *list,
    u32              vertex_count,
    u32              index_count
) {
    return list->vertex_count + vertex_count <= list->vertex_capacity
        && list->index_count + index_count <= list->index_capacity;
}

FOXUI_INTERNAL bool foxui_draw_list_push_triangle(
    Foxui_Draw_List *list,
    Foxui_Vertex     a,
    Foxui_Vertex     b,
    Foxui_Vertex     c
) {
    if(!foxui_draw_list_can_push(list, 3, 3)) {
        // todo(sora): loggin
        return false;
    }
    
    u32 base = list->vertex_count;
    
    list->vertices[list->vertex_count++] = a;
    list->vertices[list->vertex_count++] = b;
    list->vertices[list->vertex_count++] = c;
    
    list->indices[list->index_count++] = base;
    list->indices[list->index_count++] = base + 1;
    list->indices[list->index_count++] = base + 2;

    return true;
}

FOXUI_INTERNAL bool foxui_draw_list_push_quad(
    Foxui_Draw_List *list,
    Foxui_Vertex     a,
    Foxui_Vertex     b,
    Foxui_Vertex     c,
    Foxui_Vertex     d
) {
    if(!foxui_draw_list_can_push(list, 4, 6)) {
        // todo(sora): logging
        return false;
    }
    
    u32 base = list->vertex_count;
    
    list->vertices[list->vertex_count++] = a;
    list->vertices[list->vertex_count++] = b;
    list->vertices[list->vertex_count++] = c;
    list->vertices[list->vertex_count++] = d;
    
    list->indices[list->index_count++] = base;
    list->indices[list->index_count++] = base + 1;
    list->indices[list->index_count++] = base + 2;
    
    list->indices[list->index_count++] = base;
    list->indices[list->index_count++] = base + 2;
    list->indices[list->index_count++] = base + 3;

    return true;
}

FOXUI_INTERNAL bool foxui_draw_list_push_rect(
    Foxui_Draw_List *list,
    Foxui_Rect       rect,
    Foxui_Color      color
) {
    Foxui_Vertex a = foxui_vertex(rect.left, rect.top, color);
    Foxui_Vertex b = foxui_vertex(rect.right, rect.top, color);
    Foxui_Vertex c = foxui_vertex(rect.right, rect.bottom, color);
    Foxui_Vertex d = foxui_vertex(rect.left, rect.bottom, color);
    
    return foxui_draw_list_push_quad(list, a, b, c, d);
}

FOXUI_INTERNAL bool foxui_draw_list_push_line(
    Foxui_Draw_List *list,
    Foxui_Point      p0,
    Foxui_Point      p1,
    f32              thickness,
    Foxui_Color      color
) {
    if(thickness <= 0.f) {
        // todo(sora): assert, but currently i don't have a good assert macro
        return false;
    }
    

    f32 delta_x = p1.x - p0.x;
    f32 delta_y = p1.y - p0.y;
    
    f32 length = sqrtf(delta_x * delta_x + delta_y * delta_y);
    if(length == 0.f) {
        // todo(sora): logging
        return false;
    }
    
    // note(sora): half the thickness, as we have essentially 2 lines
    f32 scale = thickness * 0.5f / length;
    // note(sora): shift the line by the perpendicular offset vector
    f32 offset_x = -delta_y * scale;
    f32 offset_y = delta_x * scale;
    
    return foxui_draw_list_push_quad(
        list,
        foxui_vertex(p0.x + offset_x, p0.y + offset_y, color),
        foxui_vertex(p0.x - offset_x, p0.y - offset_y, color),
        foxui_vertex(p1.x - offset_x, p1.y - offset_y, color),
        foxui_vertex(p1.x + offset_x, p1.y + offset_y, color)
    );
}

FOXUI_INTERNAL bool foxui_draw_list_push_rect_outline(
    Foxui_Draw_List *list,
    Foxui_Rect       rect,
    f32              thickness,
    Foxui_Color      color
) {
    if(!foxui_draw_list_can_push(list, 16, 24)) {
        // todo(sora): logging
        return false;
    }
    
    if(thickness <= 0.f) {
        // todo(sora): assert, but currently i don't have a good assert macro
        return false;
    }
    
    foxui_draw_list_push_rect(
        list,
        {rect.left, rect.top, rect.right, rect.top + thickness},
        color
    );
    foxui_draw_list_push_rect(
        list,
        {rect.left, rect.bottom - thickness, rect.right, rect.bottom},
        color
    );
    foxui_draw_list_push_rect(
        list,
        {rect.left, rect.top + thickness, rect.left + thickness, rect.bottom - thickness}, 
        color
    );
    foxui_draw_list_push_rect(
        list,
        {rect.right - thickness, rect.top + thickness, rect.right, rect.bottom - thickness},
        color
    );
    
    return true;
}

void foxui_begin_draw_command(Foxui_Window *, Foxui_Draw_List *list, Foxui_Rect rect) {
    if(list->command_count >= list->command_capacity) {
        // todo(sora): logging
        return;
    }
    
    list->commands[list->command_count++] = Foxui_Draw_Command {
        .clip_rect = rect,
        .first_index = list->index_count,
        .index_count = 0,
    };
}

void foxui_end_draw_command(Foxui_Window *, Foxui_Draw_List *list) {
    if(list->command_count == 0) {
        // todo(sora): this is an assert
        return;
    }
    
    Foxui_Draw_Command *command = &list->commands[list->command_count - 1]; 
    u32 index_count             = list->index_count - command->first_index;
    command->index_count        = index_count;
}