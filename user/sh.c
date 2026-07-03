#include "libc.h"


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
    print("  open <app>         "); set_color(COLOR_GREY, COLOR_DARK); print("- Open an app (e.g. calc, hello, exploit)\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  write <f> <t>      "); set_color(COLOR_GREY, COLOR_DARK); print("- Write text string to filename\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  top                "); set_color(COLOR_GREY, COLOR_DARK); print("- Dynamic real-time process monitor\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  ps                 "); set_color(COLOR_GREY, COLOR_DARK); print("- List active processes\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  shutdown           "); set_color(COLOR_GREY, COLOR_DARK); print("- Power off the computer\n");
    set_color(COLOR_CYAN, COLOR_DARK);
    print("  restart            "); set_color(COLOR_GREY, COLOR_DARK); print("- Reboot the computer\n");
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

static void local_strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while ((*dest++ = *src++));
}


int main(void) {
    clear_screen();
    print_banner();

    char input[128];

    while (1) {
        // Print shell prompt
        set_color(COLOR_PURPLE, COLOR_DARK);
        print("zenith$ ");
        set_color(COLOR_WHITE, COLOR_DARK);
        swap_buffers();

        // Read command line
        int len = read(0, input, sizeof(input));
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
                int fd = open(filename, 0);
                if (fd < 0) {
                    set_color(0x00FF3333, COLOR_DARK);
                    print("Error: File not found or read failure.\n");
                    set_color(COLOR_WHITE, COLOR_DARK);
                } else {
                    char chunk[512];
                    int bytes_read;
                    while ((bytes_read = read(fd, chunk, sizeof(chunk) - 1)) > 0) {
                        for (int i = 0; i < bytes_read; i++) {
                            char c = chunk[i];
                            if ((c >= 32 && c < 127) || c == '\n' || c == '\r' || c == '\t') {
                                putchar(c);
                            } else {
                                putchar('.');
                            }
                        }
                    }
                    print("\n");
                    close(fd);
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
        else if (strcmp(cmd, "open") == 0) {
            char* app = strtok(NULL, " ");
            if (app == NULL) {
                set_color(0x00FF3333, COLOR_DARK);
                print("Usage: open <app_name>\n");
                set_color(COLOR_WHITE, COLOR_DARK);
            } else {
                char app_bin[64];
                strcpy(app_bin, app);
                if (strlen(app) < 4 || strcmp(app + strlen(app) - 4, ".bin") != 0) {
                    local_strcat(app_bin, ".bin");
                }
                print("Launching "); print(app_bin); print("...\n");
                int exec_res = exec(app_bin);
                if (exec_res < 0) {
                    exec_res = exec(app);
                }
                if (exec_res < 0) {
                    set_color(0x00FF3333, COLOR_DARK);
                    print("Error: Could not open "); print(app); print("\n");
                    set_color(COLOR_WHITE, COLOR_DARK);
                }
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
        else {
            set_color(0x00FF3333, COLOR_DARK);
            print("Unknown command: '");
            print(cmd);
            print("'. Type 'help' for options.\n");
            set_color(COLOR_WHITE, COLOR_DARK);
        }
        swap_buffers();
        print("\n");
    }

    return 0;
}
