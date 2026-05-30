#include "libc.h"

int main(void) {
    set_color(0x00FF80FF, 0x000F0F14); // Magenta
    print("\n********************************************\n");
    print("* Hello from a separate compiled binary!   *\n");
    print("********************************************\n");
    print("Demo program active. Sleeping for 2 seconds...\n");
    
    sleep(200); // 200 ticks = 2 seconds
    
    print("Sleep finished! Returning to shell...\n");
    
    exit();
    return 0;
}
