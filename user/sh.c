#include "libc.h"

// Theme color definitions
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_GREY    0x00E0E0E0
#define COLOR_DARK    0x000F0F14
#define COLOR_CYAN    0x0000E5FF
#define COLOR_PURPLE  0x00BF55EC
#define COLOR_MAGENTA 0x00FF80FF
#define COLOR_GREEN   0x0033FF33
#define COLOR_BLACK   0x00000000
#define COLOR_YELLOW  0x00F7CA18
#define COLOR_BLUE    0x0022A7F0

static void print_banner(void) {
    set_color(COLOR_PURPLE, COLOR_DARK);
    print("========================================================================\n");
    print("                      Zenith OS Interactive User Shell                  \n");
    print("========================================================================\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  Type 'help' to list built-in commands or enter an executable name.\n\n");
    set_color(COLOR_GREY, COLOR_DARK);
}

static void print_help(void) {
    set_color(COLOR_YELLOW, COLOR_DARK);
    print("Zenith OS Shell Help Commands:\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  help               "); set_color(COLOR_GREY, COLOR_DARK); print("- Display this help info\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  ls                 "); set_color(COLOR_GREY, COLOR_DARK); print("- List files on the ZenithFS drive\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  cat <filename>     "); set_color(COLOR_GREY, COLOR_DARK); print("- Print the text content of a file\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  clear              "); set_color(COLOR_GREY, COLOR_DARK); print("- Clear the console screen\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  theme <name>       "); set_color(COLOR_GREY, COLOR_DARK); print("- Switch color theme (default, matrix, ocean, retro)\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  calc               "); set_color(COLOR_GREY, COLOR_DARK); print("- Launch interactive calculator app\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  blaster            "); set_color(COLOR_GREY, COLOR_DARK); print("- Launch space invaders arcade game\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  exploit            "); set_color(COLOR_GREY, COLOR_DARK); print("- Launch exploit verification suite\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  hello              "); set_color(COLOR_GREY, COLOR_DARK); print("- Launch hello world demonstration\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  shutdown           "); set_color(COLOR_GREY, COLOR_DARK); print("- Power off the computer\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  restart            "); set_color(COLOR_GREY, COLOR_DARK); print("- Reboot the computer\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  write <f> <t>      "); set_color(COLOR_GREY, COLOR_DARK); print("- Write text string to filename\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  top                "); set_color(COLOR_GREY, COLOR_DARK); print("- Dynamic real-time process monitor\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  <binary.bin>       "); set_color(COLOR_GREY, COLOR_DARK); print("- Load and execute a binary from disk\n");
}


static void handle_theme(const char* name) {
    if (strcmp(name, "default") == 0) {
        set_color(COLOR_GREY, COLOR_DARK);
        clear_screen();
        print_banner();
    } else if (strcmp(name, "matrix") == 0) {
        set_color(COLOR_GREEN, COLOR_BLACK);
        clear_screen();
        print("--- Matrix Mode Activated ---\n");
    } else if (strcmp(name, "ocean") == 0) {
        set_color(COLOR_CYAN, 0x000B132B);
        clear_screen();
        print("--- Deep Ocean Theme Enabled ---\n");
    } else if (strcmp(name, "retro") == 0) {
        set_color(COLOR_YELLOW, 0x001F1F1F);
        clear_screen();
        print("--- Classic Retro Amber Active ---\n");
    } else {
        print("Unknown theme. Available: default, matrix, ocean, retro\n");
        swap_buffers();
        return;
    }
    swipe_transition();
}

int main(void) {
    clear_screen();
    print_banner();

    char input[128];
    char file_buffer[4096];

    while (1) {
        // Print shell prompt
        set_color(COLOR_PURPLE, COLOR_DARK);
        print("zenith$ ");
        set_color(COLOR_WHITE, COLOR_DARK);
        swap_buffers();

        // Read command line
        int len = read(input, sizeof(input));
        if (len <= 0) {
            print("\n");
            continue;
        }

        // Remove trailing newline character
        if (input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        char orig_input[128];
        strcpy(orig_input, input);

        // Tokenize input
        char* cmd = strtok(input, " ");
        if (cmd == NULL || strlen(cmd) == 0) {
            continue;
        }

        // 1. Help command
        if (strcmp(cmd, "help") == 0) {
            print_help();
        }
        // 2. Ls command
        else if (strcmp(cmd, "ls") == 0) {
            set_color(COLOR_CYAN, COLOR_DARK);
            list_files();
            set_color(COLOR_WHITE, COLOR_DARK);
        }
        // 3. Clear command
        else if (strcmp(cmd, "clear") == 0) {
            clear_screen();
            print_banner();
        }
        // 4. Cat command
        else if (strcmp(cmd, "cat") == 0) {
            char* filename = strtok(NULL, " ");
            if (filename == NULL) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Usage: cat <filename>\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            } else {
                memset(file_buffer, 0, sizeof(file_buffer));
                int read_size = read_file(filename, file_buffer);
                if (read_size < 0) {
                    set_color(0x00FF3333, COLOR_DARK);
                    print("Error: File not found or read failure.\n");
                    set_color(COLOR_WHITE, COLOR_DARK);
                } else {
                    file_buffer[read_size] = '\0';
                    print(file_buffer);
                    print("\n");
                }
            }
        }
        // 5. Theme command
        else if (strcmp(cmd, "theme") == 0) {
            char* theme_name = strtok(NULL, " ");
            if (theme_name == NULL) {
                print("Usage: theme <default|matrix|ocean|retro>\n");
            } else {
                handle_theme(theme_name);
            }
        }
        // 6. Calc command
        else if (strcmp(cmd, "calc") == 0) {
            print("Launching calculator...\n");
            int exec_res = exec("calc.bin");
            if (exec_res < 0) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Error: calc.bin not found on disk.\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            }
        }
        // 7. Blaster command
        else if (strcmp(cmd, "blaster") == 0) {
            print("Launching space invaders game...\n");
            int exec_res = exec("blaster.bin");
            if (exec_res < 0) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Error: blaster.bin not found on disk.\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            }
        }
        // 8. Exploit command
        else if (strcmp(cmd, "exploit") == 0) {
            print("Launching exploit verification suite...\n");
            int exec_res = exec("exploit.bin");
            if (exec_res < 0) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Error: exploit.bin not found on disk.\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            }
        }
        // 9. Hello command
        else if (strcmp(cmd, "hello") == 0) {
            print("Launching hello demo...\n");
            int exec_res = exec("hello.bin");
            if (exec_res < 0) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Error: hello.bin not found on disk.\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            }
        }
        // ps command
        else if (strcmp(cmd, "ps") == 0) {
            TaskInfo tasks[16];
            int num_tasks = get_tasks(tasks, 16);
            if (num_tasks < 0) {
                print("Failed to retrieve task listing.\n");
            } else {
                print("ID    STATE            MEMORY\n");
                print("---------------------------------\n");
                for (int i = 0; i < num_tasks; i++) {
                    print(itoa(tasks[i].id, 10));
                    int id_len = strlen(itoa(tasks[i].id, 10));
                    for (int j = id_len; j < 6; j++) print(" ");
                    
                    const char* state_str = "UNKNOWN";
                    if (tasks[i].state == 0) state_str = "READY";
                    else if (tasks[i].state == 1) state_str = "RUNNING";
                    else if (tasks[i].state == 2) state_str = "SLEEPING";
                    else if (tasks[i].state == 3) state_str = "DEAD";
                    else if (tasks[i].state == 4) state_str = "BLOCKED";
                    else if (tasks[i].state == 5) state_str = "BLOCKED_INPUT";
                    
                    print(state_str);
                    int state_len = strlen(state_str);
                    for (int j = state_len; j < 17; j++) print(" ");
                    
                    if (tasks[i].mem_size_kb == 0) {
                        print("0 KB (Kernel)\n");
                    } else {
                        print(itoa(tasks[i].mem_size_kb, 10));
                        print(" KB\n");
                    }
                }
            }
        }
        // write command
        else if (strcmp(cmd, "write") == 0) {
            char* filename = strtok(NULL, " ");
            char* text = strtok(NULL, ""); // Get all remaining characters including spaces
            if (filename == NULL || text == NULL) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Usage: write <filename> <text>\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            } else {
                int res = write_file(filename, text, strlen(text));
                if (res < 0) {
                    set_color(0x00FF3333, COLOR_DARK);
                    print("Error: Failed to write to file.\n");
                    set_color(COLOR_WHITE, COLOR_DARK);
                } else {
                    print("Successfully wrote ");
                    print(itoa(res, 10));
                    print(" bytes to ");
                    print(filename);
                    print("\n");
                }
            }
        }
        // top command
        else if (strcmp(cmd, "top") == 0) {
            clear_screen();
            while (1) {
                char c = getchar_nonblock();
                if (c == 'q' || c == 'Q') {
                    break;
                }
                
                set_cursor(0, 0);
                
                set_color(COLOR_YELLOW, COLOR_DARK);
                print("====================================================\n");
                print("           ZenithOS Dynamic Process Monitor         \n");
                print("====================================================\n");
                set_color(COLOR_CYAN, COLOR_DARK);
                print("ID    STATUS           MEMORY       UPTIME (Ticks)  \n");
                print("----------------------------------------------------\n");
                set_color(COLOR_WHITE, COLOR_DARK);
                
                TaskInfo tasks[16];
                int num_tasks = get_tasks(tasks, 16);
                if (num_tasks < 0) {
                    print("Failed to retrieve task listing.\n");
                } else {
                    for (int i = 0; i < num_tasks; i++) {
                        // Print ID
                        print(itoa(tasks[i].id, 10));
                        int id_len = strlen(itoa(tasks[i].id, 10));
                        for (int j = id_len; j < 6; j++) print(" ");
                        
                        // Print Status
                        const char* state_str = "UNKNOWN";
                        if (tasks[i].state == 0) state_str = "READY";
                        else if (tasks[i].state == 1) state_str = "RUNNING";
                        else if (tasks[i].state == 2) state_str = "SLEEPING";
                        else if (tasks[i].state == 3) state_str = "DEAD";
                        else if (tasks[i].state == 4) state_str = "BLOCKED";
                        else if (tasks[i].state == 5) state_str = "BLOCKED_INPUT";
                        
                        print(state_str);
                        int state_len = strlen(state_str);
                        for (int j = state_len; j < 17; j++) print(" ");
                        
                        // Print Memory
                        if (tasks[i].mem_size_kb == 0) {
                            print("0 KB (Kernel)");
                            for (int j = 13; j < 13; j++) print(" ");
                        } else {
                            print(itoa(tasks[i].mem_size_kb, 10));
                            print(" KB");
                            int mem_len = strlen(itoa(tasks[i].mem_size_kb, 10)) + 3;
                            for (int j = mem_len; j < 13; j++) print(" ");
                        }
                        
                        // Print Uptime
                        print(itoa(tasks[i].uptime, 10));
                        print("\n");
                    }
                }
                
                // Print prompt to exit
                set_color(COLOR_PURPLE, COLOR_DARK);
                print("\nPress 'q' to exit dynamic process monitor.\n");
                set_color(COLOR_WHITE, COLOR_DARK);
                
                swap_buffers();
                sleep(100); // Sleep 1 second (100 PIT ticks)
            }
            clear_screen();
            print_banner();
        }
        // 8. Shutdown command
        else if (strcmp(cmd, "shutdown") == 0) {
            print("Initiating system shutdown...\n");
            shutdown();
        }
        // 9. Restart command
        else if (strcmp(cmd, "restart") == 0) {
            print("Initiating system restart...\n");
            restart();
        }
        // 10. External Binary execution
        else {
            // Attempt to load and run external program from disk
            print("Loading process "); print(cmd); print("...\n");
            int exec_res = exec(orig_input);
            if (exec_res < 0) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Unknown command or binary: '");
                print(cmd);
                print("'\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            }
            // If exec succeeds, control jumps to the program, and we won't return here
            // unless the process completes and control returns. But exec resets memory,
            // so we actually restart or reboot if the program exit syscall is triggered.
        }
        swap_buffers();
        print("\n");
    }

    return 0;
}
