#include "foxui.h"

int main(void) {
    Foxui_Window window = {};
    if(!foxui_create_window(&window, {S8("Foxui Calculator"), 960, 540})) {
        return 1;
    }
    
    while(foxui_wait_events(&window)) {
        
    }
    
    foxui_destroy_window(&window);
    return 0;
}