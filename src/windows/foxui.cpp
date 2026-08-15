#include "../foxui.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

// todo(sora): probably should happen in the buildscript...
#pragma comment(lib, "dwmapi.lib")

// todo(sora): perhaps, this should also be configurable?
#define FOXUI_WINDOW_CLASS_NAME L"Foxui_Window_Class_Name"

FOXUI_INTERNAL COLORREF FOXUI_COLOR_BG                   RGB(0x2a, 0x29, 0x47);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_TITLEBAR             RGB(0x20, 0x20, 0x37);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_BORDER               RGB(0x40, 0x40, 0x70);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_TEXT                 RGB(0xa0, 0xb7, 0xeb);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_BUTTON_HOVER         RGB(0x33, 0x32, 0x5d);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_BUTTON_PRESSED       RGB(0x3b, 0x3b, 0x80);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_CLOSE_BUTTON_HOVER   RGB(0xc4, 0x2b, 0x1c);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_CLOSE_BUTTON_PRESSED RGB(0xa8, 0x25, 0x1a);
FOXUI_INTERNAL COLORREF FOXUI_COLOR_CLOSE_ICON_HOVER     RGB(0xff, 0xff, 0xff);

enum {
    FOXUI_WND_EXTRA_HOVER_TARGET             = 0,
    FOXUI_WND_EXTRA_HOVER_LAST_TICK          = sizeof(LONG_PTR),
    FOXUI_WND_EXTRA_MINIMIZE_HOVER_INTENSITY = sizeof(LONG_PTR) * 2,
    FOXUI_WND_EXTRA_MAXIMIZE_HOVER_INTENSITY = sizeof(LONG_PTR) * 3,
    FOXUI_WND_EXTRA_CLOSE_HOVER_INTENSITY    = sizeof(LONG_PTR) * 4,
    FOXUI_WND_EXTRA_SIZE                     = sizeof(LONG_PTR) * 5,
};

enum {
    FOXUI_HOVER_TIMER_ID = 1,
    FOXUI_HOVER_TIMER_MS = 15,
    FOXUI_HOVER_FADE_MS  = 160,
};

// todo(sora): probably, we should also be able to configure this to be turned off
// note(sora): for older SDKs; Win11 DWM attributes
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

FOXUI_INTERNAL s32 foxui_scale_for_dpi(s32 value, u32 dpi) {
    return MulDiv(value, (s32)dpi, 96);
}

FOXUI_INTERNAL usize foxui_utf16_from_utf8(
    String8 source,
    wchar_t *destination,
    usize    destination_capacity
) {
    if (!destination || destination_capacity == 0 || destination_capacity > 0x7FFFFFFF ||
        (!source.items && source.count) || source.count > 0x7FFFFFFF) {
        return 0;
    }

    destination[0] = 0;
    if (source.count == 0)
        return 0;

    s32 converted_count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        (char *)source.items,
        (s32)source.count,
        destination,
        (s32)destination_capacity - 1
    );
    if (converted_count <= 0)
        return 0;

    destination[converted_count] = 0;
    return (usize)converted_count;
}

FOXUI_INTERNAL u32 foxui_window_dpi(HWND wnd) {
    u32 dpi = wnd ? GetDpiForWindow(wnd) : GetDpiForSystem();
    return dpi ? dpi : 96;
}

FOXUI_INTERNAL Foxui_Window *foxui_window_from_native(HWND native_window) {
    return (Foxui_Window *)GetWindowLongPtrW(native_window, GWLP_USERDATA);
}

enum Foxui_Title_Button {
    FOXUI_TITLE_BUTTON_NONE = 0,
    FOXUI_TITLE_BUTTON_MINIMIZE,
    FOXUI_TITLE_BUTTON_MAXIMIZE,
    FOXUI_TITLE_BUTTON_CLOSE,
    FOXUI_TITLE_BUTTON_COUNT,
};

FOXUI_INTERNAL Foxui_Title_Button FOXUI_TITLE_BUTTONS[] = {
    FOXUI_TITLE_BUTTON_MINIMIZE,
    FOXUI_TITLE_BUTTON_MAXIMIZE,
    FOXUI_TITLE_BUTTON_CLOSE,
};

FOXUI_INTERNAL s32 foxui_title_button_hover_intensity_slot(Foxui_Title_Button button) {
    switch (button) {
        case FOXUI_TITLE_BUTTON_MINIMIZE: return FOXUI_WND_EXTRA_MINIMIZE_HOVER_INTENSITY;
        case FOXUI_TITLE_BUTTON_MAXIMIZE: return FOXUI_WND_EXTRA_MAXIMIZE_HOVER_INTENSITY;
        case FOXUI_TITLE_BUTTON_CLOSE:    return FOXUI_WND_EXTRA_CLOSE_HOVER_INTENSITY;
        default:                          return 0;
    }
}

FOXUI_INTERNAL Foxui_Title_Button foxui_title_button_at(HWND hwnd, s32 x, s32 y) {
    RECT client_rect = {};
    GetClientRect(hwnd, &client_rect);
    
    u32 dpi             = foxui_window_dpi(hwnd);
    s32 titlebar_height = foxui_scale_for_dpi(32, dpi);
    s32 button_width    = foxui_scale_for_dpi(46, dpi);
    
    if(y < 0 || y >= titlebar_height || x < 0 || x >= client_rect.right) {
        return FOXUI_TITLE_BUTTON_NONE;
    }
    if(x >= client_rect.right - button_width) {
        return FOXUI_TITLE_BUTTON_CLOSE;
    }
    if(x >= client_rect.right - button_width * 2) {
        return FOXUI_TITLE_BUTTON_MAXIMIZE;
    }
    if(x >= client_rect.right - button_width * 3) {
        return FOXUI_TITLE_BUTTON_MINIMIZE;
    }
    
    return FOXUI_TITLE_BUTTON_NONE;
}

FOXUI_INTERNAL void foxui_invalidate_title_button(
    HWND               native_window,
    Foxui_Title_Button button
) {
    if (button == FOXUI_TITLE_BUTTON_NONE) return;

    RECT client_rect = {};
    GetClientRect(native_window, &client_rect);

    u32 dpi = foxui_window_dpi(native_window);
    s32 button_width = foxui_scale_for_dpi(46, dpi);
    s32 index_from_right = 3;
    switch(button) {
        case FOXUI_TITLE_BUTTON_MAXIMIZE: {
            index_from_right = 2;
        } break;
        case FOXUI_TITLE_BUTTON_CLOSE: {
            index_from_right = 1;
        } break;
        default: break;
    }
    
    RECT button_rect = {
        client_rect.right - button_width * index_from_right,
        0,
        client_rect.right - button_width * (index_from_right - 1),
        foxui_scale_for_dpi(32, dpi),
    };
    InvalidateRect(native_window, &button_rect, FALSE);
}

FOXUI_INTERNAL void foxui_invalidate_title_button_transition(
    HWND               hwnd,
    Foxui_Title_Button hovered_button
) {
    Foxui_Title_Button previous_hovered_button = (Foxui_Title_Button)GetWindowLongPtrW(
        hwnd,
        FOXUI_WND_EXTRA_HOVER_TARGET
    );
    if (previous_hovered_button == hovered_button)
        return;

    foxui_invalidate_title_button(hwnd, previous_hovered_button);
    foxui_invalidate_title_button(hwnd, hovered_button);
}

FOXUI_INTERNAL void foxui_invalidate_animated_title_buttons(HWND hwnd) {
    Foxui_Title_Button target_button = (Foxui_Title_Button)GetWindowLongPtrW(
        hwnd,
        FOXUI_WND_EXTRA_HOVER_TARGET
    );
    for (Foxui_Title_Button button : FOXUI_TITLE_BUTTONS) {
        s32 intensity_slot = foxui_title_button_hover_intensity_slot(button);
        u32 current_intensity = (u32)GetWindowLongPtrW(hwnd, intensity_slot);
        u32 target_intensity = button == target_button ? 255 : 0;
        if (current_intensity != target_intensity)
            foxui_invalidate_title_button(hwnd, button);
    }
}

FOXUI_INTERNAL COLORREF foxui_blend_color(COLORREF from, COLORREF to, u32 t) {
    u32 r = (GetRValue(from) * (255 - t) + GetRValue(to) * t) / 255;
    u32 g = (GetGValue(from) * (255 - t) + GetGValue(to) * t) / 255;
    u32 b = (GetBValue(from) * (255 - t) + GetBValue(to) * t) / 255;
    return RGB(r, g, b);
}

FOXUI_INTERNAL LRESULT foxui_window_hit_test(Foxui_Window *window, LPARAM mouse_position) {
    HWND hwnd        = (HWND)window->native_window;
    POINT cursor     = {GET_X_LPARAM(mouse_position), GET_Y_LPARAM(mouse_position)};
    RECT window_rect = {};
    GetWindowRect(hwnd, &window_rect);

    u32 dpi           = foxui_window_dpi(hwnd);
    s32 resize_width  = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                        GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    s32 resize_height = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                        GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

    if (!IsZoomed(hwnd)) {
        bool on_left   = cursor.x < window_rect.left + resize_width;
        bool on_right  = cursor.x >= window_rect.right - resize_width;
        bool on_top    = cursor.y < window_rect.top + resize_height;
        bool on_bottom = cursor.y >= window_rect.bottom - resize_height;

        if (on_top && on_left)
            return HTTOPLEFT;
        if (on_top && on_right)
            return HTTOPRIGHT;
        if (on_bottom && on_left)
            return HTBOTTOMLEFT;
        if (on_bottom && on_right)
            return HTBOTTOMRIGHT;
        if (on_left)
            return HTLEFT;
        if (on_right)
            return HTRIGHT;
        if (on_top)
            return HTTOP;
        if (on_bottom)
            return HTBOTTOM;
    }

    POINT client_cursor = cursor;
    ScreenToClient(hwnd, &client_cursor);

    Foxui_Title_Button current_button = foxui_title_button_at(
        hwnd,
        client_cursor.x,
        client_cursor.y
    );
    
    bool on_titlebar = client_cursor.y < window->titlebar_rect.bottom
        && client_cursor.y > window->titlebar_rect.top
        && client_cursor.x > window->titlebar_rect.left
        && client_cursor.x < window->titlebar_rect.right
        && current_button == FOXUI_TITLE_BUTTON_NONE;

    if(on_titlebar) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

FOXUI_INTERNAL void foxui_draw_titlebar(Foxui_Window *window) {
    HWND hwnd = (HWND)window->native_window;
    PAINTSTRUCT paint = {};
    HDC device_context = BeginPaint(hwnd, &paint);
    if (!device_context) return;

    RECT client_rect = {};
    GetClientRect(hwnd, &client_rect);

    u32 dpi             = foxui_window_dpi(hwnd);
    s32 titlebar_height = foxui_scale_for_dpi(32, dpi);
    s32 button_width    = foxui_scale_for_dpi(46, dpi);

    // note(sora): hover/press derived at paint time; fade state lives in wndExtra slots
    POINT cursor = {};
    GetCursorPos(&cursor);
    ScreenToClient(hwnd, &cursor);
    Foxui_Title_Button hovered = foxui_title_button_at(hwnd, cursor.x, cursor.y);
    Foxui_Title_Button pressed = (GetCapture() == hwnd && hovered &&
                                  (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                                     ? hovered
                                     : FOXUI_TITLE_BUTTON_NONE;

    u64 current_tick = GetTickCount64();
    LONG_PTR stored_target = GetWindowLongPtrW(hwnd, FOXUI_WND_EXTRA_HOVER_TARGET);
    if (stored_target != (LONG_PTR)hovered) {
        SetWindowLongPtrW(hwnd, FOXUI_WND_EXTRA_HOVER_TARGET, (LONG_PTR)hovered);
        SetWindowLongPtrW(hwnd, FOXUI_WND_EXTRA_HOVER_LAST_TICK, (LONG_PTR)current_tick);
    }

    u64 previous_tick = (u64)GetWindowLongPtrW(hwnd, FOXUI_WND_EXTRA_HOVER_LAST_TICK);
    u64 elapsed_ms = previous_tick ? current_tick - previous_tick : 0;
    u64 intensity_step_wide = elapsed_ms * 255 / FOXUI_HOVER_FADE_MS;
    u32 intensity_step = intensity_step_wide >= 255 ? 255 : (u32)intensity_step_wide;
    SetWindowLongPtrW(hwnd, FOXUI_WND_EXTRA_HOVER_LAST_TICK, (LONG_PTR)current_tick);

    u32 button_hover_intensities[FOXUI_TITLE_BUTTON_COUNT] = {};
    bool is_hover_animation_active = false;
    for (Foxui_Title_Button button : FOXUI_TITLE_BUTTONS) {
        s32 intensity_slot = foxui_title_button_hover_intensity_slot(button);
        u32 current_intensity = (u32)GetWindowLongPtrW(hwnd, intensity_slot);
        u32 target_intensity = hovered == button ? 255 : 0;

        if (current_intensity < target_intensity) {
            u32 remaining_intensity = target_intensity - current_intensity;
            current_intensity += intensity_step < remaining_intensity
                ? intensity_step
                : remaining_intensity;
        } else if (current_intensity > target_intensity) {
            u32 intensity_reduction = intensity_step < current_intensity
                ? intensity_step
                : current_intensity;
            current_intensity -= intensity_reduction;
        }

        button_hover_intensities[button] = current_intensity;
        SetWindowLongPtrW(hwnd, intensity_slot, (LONG_PTR)current_intensity);
        is_hover_animation_active |= current_intensity != target_intensity;
    }

    if (is_hover_animation_active)
        SetTimer(hwnd, FOXUI_HOVER_TIMER_ID, FOXUI_HOVER_TIMER_MS, nullptr);
    else
        KillTimer(hwnd, FOXUI_HOVER_TIMER_ID);

    HBRUSH background_brush = CreateSolidBrush(FOXUI_COLOR_BG);
    HBRUSH titlebar_brush   = CreateSolidBrush(FOXUI_COLOR_TITLEBAR);
    FillRect(device_context, &client_rect, background_brush);

    RECT titlebar_rect = client_rect;
    titlebar_rect.bottom = titlebar_height;
    window->titlebar_rect = Foxui_Titlebar_Rect {
        .left = titlebar_rect.left,
        .top = titlebar_rect.top,
        .right = titlebar_rect.right,
        .bottom = titlebar_rect.bottom,
    };
    FillRect(device_context, &titlebar_rect, titlebar_brush);

    for (Foxui_Title_Button button : FOXUI_TITLE_BUTTONS) {
        u32 hover_intensity = button_hover_intensities[button];
        if (hover_intensity == 0 && pressed != button) continue;

        s32 index_from_right = 3;
        switch(button) {
            case FOXUI_TITLE_BUTTON_CLOSE: {
                index_from_right = 1;
            } break;
            case FOXUI_TITLE_BUTTON_MAXIMIZE: {
                index_from_right = 2;
            } break;
            default: break;
        }
        
        RECT button_rect = {
            client_rect.right - button_width * index_from_right,
            0,
            client_rect.right - button_width * (index_from_right - 1),
            titlebar_height,
        };

        COLORREF hover_color = button == FOXUI_TITLE_BUTTON_CLOSE
            ? FOXUI_COLOR_CLOSE_BUTTON_HOVER
            : FOXUI_COLOR_BUTTON_HOVER;
        COLORREF pressed_color = button == FOXUI_TITLE_BUTTON_CLOSE
            ? FOXUI_COLOR_CLOSE_BUTTON_PRESSED
            : FOXUI_COLOR_BUTTON_PRESSED;
        COLORREF fill = pressed == button
            ? pressed_color
            : foxui_blend_color(FOXUI_COLOR_TITLEBAR, hover_color, hover_intensity);
        HBRUSH button_brush = CreateSolidBrush(fill);
        FillRect(device_context, &button_rect, button_brush);
        DeleteObject(button_brush);
    }

    HPEN icon_pen = CreatePen(PS_SOLID, foxui_scale_for_dpi(1, dpi), FOXUI_COLOR_TEXT);
    HGDIOBJ previous_pen = SelectObject(device_context, icon_pen);
    HGDIOBJ previous_brush = SelectObject(device_context, GetStockObject(NULL_BRUSH));

    s32 icon_half_size = foxui_scale_for_dpi(5, dpi);
    s32 icon_center_y  = titlebar_height / 2;

    s32 minimize_center_x = client_rect.right - button_width * 5 / 2;
    s32 minimize_center_y = icon_center_y + (pressed == FOXUI_TITLE_BUTTON_MINIMIZE ? 1 : 0);
    MoveToEx(device_context, minimize_center_x - icon_half_size, minimize_center_y, nullptr);
    LineTo(device_context, minimize_center_x + icon_half_size, minimize_center_y);

    s32 maximize_center_x = client_rect.right - button_width * 3 / 2;
    s32 maximize_center_y = icon_center_y + (pressed == FOXUI_TITLE_BUTTON_MAXIMIZE ? 1 : 0);
    if (IsZoomed(hwnd)) {
        s32 restore_icon_offset = foxui_scale_for_dpi(2, dpi);
        s32 back_left   = maximize_center_x - icon_half_size + restore_icon_offset;
        s32 back_top    = maximize_center_y - icon_half_size;
        s32 back_right  = maximize_center_x + icon_half_size;
        s32 back_bottom = maximize_center_y + icon_half_size - restore_icon_offset;
        s32 front_left   = maximize_center_x - icon_half_size;
        s32 front_top    = maximize_center_y - icon_half_size + restore_icon_offset;
        s32 front_right  = maximize_center_x + icon_half_size - restore_icon_offset;
        s32 front_bottom = maximize_center_y + icon_half_size;

        MoveToEx(device_context, back_left, front_top, nullptr);
        LineTo(device_context, back_left, back_top);
        LineTo(device_context, back_right, back_top);
        LineTo(device_context, back_right, back_bottom);
        LineTo(device_context, front_right, back_bottom);
        Rectangle(
            device_context,
            front_left,
            front_top,
            front_right + 1,
            front_bottom + 1
        );
    } else {
        Rectangle(
            device_context,
            maximize_center_x - icon_half_size,
            maximize_center_y - icon_half_size,
            maximize_center_x + icon_half_size + 1,
            maximize_center_y + icon_half_size + 1
        );
    }

    COLORREF close_icon_color = foxui_blend_color(
        FOXUI_COLOR_TEXT,
        FOXUI_COLOR_CLOSE_ICON_HOVER,
        pressed == FOXUI_TITLE_BUTTON_CLOSE
            ? 255
            : button_hover_intensities[FOXUI_TITLE_BUTTON_CLOSE]
    );
    HPEN close_icon_pen = CreatePen(PS_SOLID, foxui_scale_for_dpi(1, dpi), close_icon_color);
    SelectObject(device_context, close_icon_pen);

    s32 close_center_x = client_rect.right - button_width / 2;
    s32 close_center_y = icon_center_y + (pressed == FOXUI_TITLE_BUTTON_CLOSE ? 1 : 0);
    MoveToEx(
        device_context,
        close_center_x - icon_half_size,
        close_center_y - icon_half_size,
        nullptr
    );
    LineTo(device_context, close_center_x + icon_half_size, close_center_y + icon_half_size);
    MoveToEx(
        device_context,
        close_center_x + icon_half_size,
        close_center_y - icon_half_size,
        nullptr
    );
    LineTo(device_context, close_center_x - icon_half_size, close_center_y + icon_half_size);

    SelectObject(device_context, previous_brush);
    SelectObject(device_context, previous_pen);
    DeleteObject(close_icon_pen);
    DeleteObject(icon_pen);
    DeleteObject(titlebar_brush);
    DeleteObject(background_brush);
    EndPaint(hwnd, &paint);
}

FOXUI_INTERNAL LRESULT CALLBACK foxui_wnd_proc(
    HWND   hwnd,
    UINT   message,
    WPARAM word_parameter,
    LPARAM long_parameter
) {
    Foxui_Window *window = foxui_window_from_native(hwnd);
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *creation = (CREATESTRUCTW *)long_parameter;
        window = (Foxui_Window *)creation->lpCreateParams;
        if (!window) return FALSE;

        window->native_window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    if (!window) return DefWindowProcW(hwnd, message, word_parameter, long_parameter);

    bool custom_decorations = !window->flags.native_decorations;

    switch (message) {
        case WM_NCCALCSIZE: {
            if (custom_decorations && word_parameter) {
                NCCALCSIZE_PARAMS *client_layout = (NCCALCSIZE_PARAMS *)long_parameter;
                if (!IsZoomed(hwnd)) {
                    client_layout->rgrc[0].top += foxui_scale_for_dpi(
                        1,
                        foxui_window_dpi(hwnd)
                    );
                }
                return 0;
            }
        } break;
        case WM_NCHITTEST: {
            if (custom_decorations) return foxui_window_hit_test(window, long_parameter);
        } break;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *size_limits = (MINMAXINFO *)long_parameter;
            MONITORINFO monitor = {sizeof(monitor)};
            GetMonitorInfoW(
                MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                &monitor
            );
            size_limits->ptMaxPosition = {
                monitor.rcWork.left - monitor.rcMonitor.left,
                monitor.rcWork.top - monitor.rcMonitor.top,
            };
            size_limits->ptMaxSize = {
                monitor.rcWork.right - monitor.rcWork.left,
                monitor.rcWork.bottom - monitor.rcWork.top,
            };
            u32 dpi = foxui_window_dpi(hwnd);
            size_limits->ptMinTrackSize = {
                foxui_scale_for_dpi(180, dpi),
                foxui_scale_for_dpi(80, dpi),
            };
            return 0;
        }
        case WM_DPICHANGED: {
            window->dpi = HIWORD(word_parameter);
            RECT *suggested_rect = (RECT *)long_parameter;
            SetWindowPos(
                hwnd,
                nullptr,
                suggested_rect->left,
                suggested_rect->top,
                suggested_rect->right - suggested_rect->left,
                suggested_rect->bottom - suggested_rect->top,
                SWP_NOACTIVATE | SWP_NOZORDER
            );
            return 0;
        }
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tracking = {sizeof(tracking)};
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = hwnd;
            TrackMouseEvent(&tracking);
            Foxui_Title_Button hovered_button = foxui_title_button_at(
                hwnd,
                GET_X_LPARAM(long_parameter),
                GET_Y_LPARAM(long_parameter)
            );
            foxui_invalidate_title_button_transition(hwnd, hovered_button);
        } break;
        case WM_MOUSELEAVE: {
            foxui_invalidate_title_button_transition(hwnd, FOXUI_TITLE_BUTTON_NONE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            Foxui_Title_Button pressed_button = foxui_title_button_at(
                hwnd,
                GET_X_LPARAM(long_parameter),
                GET_Y_LPARAM(long_parameter)
            );
            if (pressed_button) SetCapture(hwnd);
            foxui_invalidate_title_button(hwnd, pressed_button);
        } return 0;
        case WM_LBUTTONUP: {
            if (GetCapture() == hwnd) {
                ReleaseCapture();
                switch (foxui_title_button_at(
                        hwnd,
                        GET_X_LPARAM(long_parameter),
                        GET_Y_LPARAM(long_parameter))
                ) {
                    case FOXUI_TITLE_BUTTON_NONE: break;
                    case FOXUI_TITLE_BUTTON_MINIMIZE: {
                        ShowWindow(hwnd, SW_MINIMIZE);
                    } break;
                    case FOXUI_TITLE_BUTTON_MAXIMIZE: {
                        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                    } break;
                    case FOXUI_TITLE_BUTTON_CLOSE: {
                        PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    } break;
                    default: break;
                }
            }
            Foxui_Title_Button hovered_button = foxui_title_button_at(
                hwnd,
                GET_X_LPARAM(long_parameter),
                GET_Y_LPARAM(long_parameter)
            );
            foxui_invalidate_title_button(hwnd, hovered_button);
            return 0;
        }
        case WM_SIZE: {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_ACTIVATE: {
            for (Foxui_Title_Button button : FOXUI_TITLE_BUTTONS)
                foxui_invalidate_title_button(hwnd, button);
            return 0;
        }
        case WM_ERASEBKGND: {
            return TRUE;
        }
        case WM_PAINT: {
            if (custom_decorations) {
                foxui_draw_titlebar(window);
                return 0;
            }
            ValidateRect(hwnd, nullptr);
            return 0;
        }
        case WM_CLOSE:
        case WM_DESTROY: {
            window->flags.should_close = 1;
            return 0;
        }
        case WM_TIMER: {
            if (word_parameter != FOXUI_HOVER_TIMER_ID) break;
            foxui_invalidate_animated_title_buttons(hwnd);
            return 0;
        }
        case WM_NCDESTROY: {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            window->native_window = nullptr;
            return DefWindowProcW(hwnd, message, word_parameter, long_parameter);
        }
    }

    return DefWindowProcW(hwnd, message, word_parameter, long_parameter);
}

bool foxui_create_window(
    Foxui_Window            *window,
    Foxui_Window_Description description
) {
    if(!window
        || (!description.title.items && description.title.count)
        || description.width <= 0
        || description.height <= 0
    ) {
        return false;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class = {sizeof(window_class)};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = foxui_wnd_proc;
    window_class.cbWndExtra = FOXUI_WND_EXTRA_SIZE;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = FOXUI_WINDOW_CLASS_NAME;
    
    if(!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    
    wchar_t title[256] = {0};
    if(description.title.count > 0
        && !foxui_utf16_from_utf8(description.title, title, FOXUI_ARRAY_COUNT(title))
    ) {
        return false;
    }
    
    u32 dpi = foxui_window_dpi(nullptr);
    s32 width = foxui_scale_for_dpi(description.width, dpi);
    s32 height = foxui_scale_for_dpi(description.height, dpi);
    DWORD style = WS_OVERLAPPEDWINDOW;
    
    HWND wnd = CreateWindowExW(
        0,
        FOXUI_WINDOW_CLASS_NAME,
        title,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        window
    );

    if(!wnd) {
        *window = {0};
        return false;
    }

    window->native_window = (void *)wnd;
    window->dpi = (u32)GetDpiForWindow(wnd);

    DWORD corner_preference = window->flags.round_corners ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(
        wnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &corner_preference,
        sizeof(corner_preference)
    );
    DwmSetWindowAttribute(
        wnd,
        DWMWA_BORDER_COLOR,
        &FOXUI_COLOR_BORDER,
        sizeof(FOXUI_COLOR_BORDER)
    );

    ShowWindow(wnd, SW_SHOWNORMAL);
    UpdateWindow(wnd);
    return true;
}

void foxui_destroy_window(Foxui_Window *window) {
    if (!window) return;
    if (window->native_window) DestroyWindow((HWND)window->native_window);

    *window = {0};
}

bool foxui_poll_events(Foxui_Window *window) {
    if (!window) return false;

    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT)
            window->flags.should_close = true;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    
    return window->native_window && !window->flags.should_close;
}

bool foxui_wait_events(Foxui_Window *window) {
    if (!window || !window->native_window || window->flags.should_close) return false;
    
    WaitMessage();
    return foxui_poll_events(window);
}
