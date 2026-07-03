#include "libc.h"

int main(int argc, char** argv) {
    set_color(COLOR_MAGENTA, COLOR_DARK);
    print("\n********************************************\n");
    print("* Hello from a separate compiled binary!   *\n");
    print("********************************************\n");
    
    print("Arguments passed:\n");
    for (int i = 0; i < argc; i++) {
        print("  argv[");
        print(itoa(i, 10));
        print("]: ");
        print(argv[i]);
        print("\n");
    }
    
    print("Demo program active. Sleeping for 2 seconds...\n");
    swap_buffers();
    
    sleep(200); // 200 ticks = 2 seconds
    
    print("Sleep finished! Returning to shell...\n");
    
    exit();
    return 0;
}
