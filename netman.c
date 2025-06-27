#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_ITEMS 50
#define MAX_LEN 256
#define CMD_BUF_SIZE 1024 // Increased buffer size for commands

// --- Function Prototypes ---
void init_ncurses();
void cleanup_ncurses();
void draw_menu(WINDOW *win, const char *title, const char **choices, int n_choices, int highlight, int start_y, int start_x);
void popup_message(const char *title, const char *message);
int get_input_from_dialog(const char *title, const char *prompt, char *result, int is_password);
int execute_command_and_parse(const char *command, char **output_lines, int max_lines);
int display_list_and_get_selection(const char *title, char **items, int count, char *selected_item);
void main_menu_loop();
void wifi_manager_loop();
void bluetooth_manager_loop();

// --- UI & Drawing ---

void init_ncurses() {
    initscr();
    clear();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_CYAN);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_GREEN, COLOR_BLACK);
    }
}

void cleanup_ncurses() {
    endwin();
}

void draw_menu(WINDOW *win, const char *title, const char **choices, int n_choices, int highlight, int start_y, int start_x) {
    werase(win);
    box(win, 0, 0);

    // Title
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 1, (getmaxx(win) - strlen(title)) / 2, "%s", title);
    wattroff(win, A_BOLD | COLOR_PAIR(1));
    
    // Instructions
    wattron(win, A_DIM);
    mvwprintw(win, getmaxy(win) - 2, 2, "Use j/k or Arrows to navigate, Enter to select, q/ESC to go back.");
    wattroff(win, A_DIM);

    // Menu items
    for (int i = 0; i < n_choices; ++i) {
        if (highlight == i) {
            wattron(win, COLOR_PAIR(2));
            mvwprintw(win, start_y + i, start_x, " > %s ", choices[i]);
            wattroff(win, COLOR_PAIR(2));
        } else {
            mvwprintw(win, start_y + i, start_x, "   %s ", choices[i]);
        }
    }
    wrefresh(win);
}

void popup_message(const char *title, const char *message) {
    int height = 8, width = 60;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;

    WINDOW *popup = newwin(height, width, starty, startx);
    box(popup, 0, 0);

    wattron(popup, A_BOLD | COLOR_PAIR(1));
    mvwprintw(popup, 1, (width - strlen(title)) / 2, "%s", title);
    wattroff(popup, A_BOLD | COLOR_PAIR(1));
    
    mvwprintw(popup, 3, 2, "%s", message);
    mvwprintw(popup, 5, 2, "Press any key to continue...");

    wrefresh(popup);
    wgetch(popup);

    delwin(popup);
    touchwin(stdscr);
    refresh();
}

// --- System Command Execution ---

int get_input_from_dialog(const char *title, const char *prompt, char *result, int is_password) {
    char command[CMD_BUF_SIZE]; // Use larger buffer
    char temp_file[] = "/tmp/netman_dialog_out.XXXXXX";
    int fd = mkstemp(temp_file);
    close(fd);

    const char* dialog_type = is_password ? "--passwordbox" : "--inputbox";
    snprintf(command, sizeof(command), "dialog --stdout --title \"%s\" %s \"%s\" 10 60", title, dialog_type, prompt);

    // Temporarily end ncurses mode to use dialog
    cleanup_ncurses();
    
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        init_ncurses();
        return -1;
    }
    
    if(fgets(result, MAX_LEN, pipe) == NULL) {
        result[0] = '\0'; // No input
    } else {
        result[strcspn(result, "\n")] = 0; // Remove newline
    }

    pclose(pipe);

    // Resume ncurses
    init_ncurses();
    refresh();
    return 0;
}

int execute_command_and_parse(const char *command, char **output_lines, int max_lines) {
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        popup_message("Error", "Failed to execute command.");
        return 0;
    }

    char line[MAX_LEN];
    int count = 0;
    while (fgets(line, sizeof(line), pipe) != NULL && count < max_lines) {
        line[strcspn(line, "\n")] = 0; // Remove newline
        // Skip headers or empty lines
        if (strlen(line) > 2 && !strstr(line, "Device") && !strstr(line, "Station")) {
            output_lines[count] = strdup(line);
            count++;
        }
    }

    pclose(pipe);
    return count;
}


int display_list_and_get_selection(const char *title, char **items, int count, char *selected_item) {
    if (count == 0) {
        popup_message(title, "No items found.");
        return -1;
    }

    WINDOW *list_win = newwin(LINES - 4, COLS - 4, 2, 2);
    keypad(list_win, TRUE);
    int highlight = 0;
    int choice = -1;

    while (1) {
        draw_menu(list_win, title, (const char **)items, count, highlight, 3, 4);
        int c = wgetch(list_win);
        switch (c) {
            case 'k':
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case 'j':
            case KEY_DOWN:
                if (highlight < count - 1) highlight++;
                break;
            case 10: // Enter
                choice = highlight;
                break;
            case 'q':
            case 27: // ESC
                choice = -1;
                break;
        }
        if (choice != -2) break;
    }
    
    if (choice != -1) {
        strncpy(selected_item, items[choice], MAX_LEN - 1);
        selected_item[MAX_LEN - 1] = '\0';
    }

    delwin(list_win);
    touchwin(stdscr);
    refresh();
    return choice;
}

// --- Manager Loops ---

void wifi_manager_loop() {
    const char *choices[] = {
        "1. Scan for Networks",
        "2. List & Connect to a Network",
        "3. Show Current Connection",
        "4. Disconnect",
        "5. Forget a Network",
        "6. Back to Main Menu"
    };
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0;
    int choice = 0;

    WINDOW *wifi_win = newwin(LINES, COLS, 0, 0);
    keypad(wifi_win, TRUE);

    while (choice != n_choices - 1) {
        draw_menu(wifi_win, "Wi-Fi Manager", choices, n_choices, highlight, (LINES - n_choices) / 2, (COLS - 40) / 2);
        int c = wgetch(wifi_win);
        switch (c) {
            case 'k':
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case 'j':
            case KEY_DOWN:
                if (highlight < n_choices - 1) highlight++;
                break;
            case 10:
                choice = highlight;
                char command[CMD_BUF_SIZE], output[4096], selected_item[MAX_LEN]; // Use larger buffer
                char *lines[MAX_ITEMS];
                int count;

                switch (choice) {
                    case 0: // Scan
                        popup_message("Scanning...", "Scanning for Wi-Fi networks...");
                        system("iwctl station wlan0 scan > /dev/null 2>&1");
                        popup_message("Scan Complete", "Network scan finished.");
                        break;
                    case 1: // List & Connect
                        count = execute_command_and_parse("iwctl station wlan0 get-networks | awk '{$1=$1;print substr($0, 6)}'", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Available Wi-Fi Networks", lines, count, selected_item) != -1) {
                            char ssid[MAX_LEN], password[MAX_LEN];
                            sscanf(selected_item, "%*s %*s %s", ssid); // Extract SSID
                            
                            get_input_from_dialog("Password Required", ssid, password, 1);
                            
                            if (strlen(password) > 0) {
                                snprintf(command, sizeof(command), "iwctl station wlan0 connect \"%s\" --passphrase \"%s\"", ssid, password);
                            } else {
                                snprintf(command, sizeof(command), "iwctl station wlan0 connect \"%s\"", ssid);
                            }
                            popup_message("Connecting...", command);
                            if (system(command) == 0) {
                                popup_message("Success", "Connected successfully.");
                            } else {
                                popup_message("Failure", "Failed to connect.");
                            }
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                    case 2: // Status
                        system("iwctl station wlan0 show > /tmp/netman_status");
                        FILE *f = fopen("/tmp/netman_status", "r");
                        fread(output, 1, sizeof(output)-1, f);
                        fclose(f);
                        remove("/tmp/netman_status");
                        popup_message("Wi-Fi Status", output);
                        break;
                    case 3: // Disconnect
                        system("iwctl station wlan0 disconnect");
                        popup_message("Wi-Fi", "Disconnected from network.");
                        break;
                    case 4: // Forget Network
                        count = execute_command_and_parse("iwctl known-networks list | awk '{print $2}'", lines, MAX_ITEMS);
                         if (display_list_and_get_selection("Forget a Network", lines, count, selected_item) != -1) {
                            snprintf(command, sizeof(command), "iwctl known-networks %s forget", selected_item);
                            system(command);
                            popup_message("Success", "Network forgotten.");
                         }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                }
                // Reset choice to avoid loop exit unless 'Back' is chosen
                if (choice != n_choices - 1) choice = -1; 
                break;
            case 'q':
            case 27:
                choice = n_choices - 1;
                break;
        }
    }
    delwin(wifi_win);
}

void bluetooth_manager_loop() {
    const char *choices[] = {
        "1. Power On/Off",
        "2. Scan for Devices",
        "3. List/Pair/Connect to Device",
        "4. Disconnect from a Device",
        "5. Back to Main Menu"
    };
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0;
    int choice = 0;

    WINDOW *bt_win = newwin(LINES, COLS, 0, 0);
    keypad(bt_win, TRUE);

    while (choice != n_choices - 1) {
        draw_menu(bt_win, "Bluetooth Manager", choices, n_choices, highlight, (LINES - n_choices) / 2, (COLS - 40) / 2);
        int c = wgetch(bt_win);
        switch (c) {
            case 'k':
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case 'j':
            case KEY_DOWN:
                if (highlight < n_choices - 1) highlight++;
                break;
            case 10:
                choice = highlight;
                char command[CMD_BUF_SIZE], selected_item[MAX_LEN]; // Use larger buffer
                char *lines[MAX_ITEMS];
                int count;

                switch (choice) {
                    case 0: // Power Toggle
                        system("echo 'show' | bluetoothctl | grep 'Powered: yes' > /tmp/bt_power");
                        FILE *f = fopen("/tmp/bt_power", "r");
                        fseek(f, 0, SEEK_END);
                        long fsize = ftell(f);
                        fclose(f);
                        remove("/tmp/bt_power");
                        if (fsize > 0) {
                            system("echo 'power off' | bluetoothctl");
                            popup_message("Bluetooth", "Powered OFF.");
                        } else {
                            system("echo 'power on' | bluetoothctl");
                            popup_message("Bluetooth", "Powered ON.");
                        }
                        break;
                    case 1: // Scan
                        popup_message("Scanning...", "Scanning for devices for 10s...");
                        system("echo 'scan on' | bluetoothctl & sleep 10 && kill -9 $! > /dev/null 2>&1");
                        system("echo 'scan off' | bluetoothctl > /dev/null 2>&1");
                        popup_message("Scan Complete", "Device scan finished.");
                        break;
                    case 2: // List/Pair/Connect
                        count = execute_command_and_parse("echo 'devices' | bluetoothctl", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Available Bluetooth Devices", lines, count, selected_item) != -1) {
                            char mac[18];
                            sscanf(selected_item, "%s", mac); // Extract MAC
                            const char *actions[] = {"Pair", "Connect", "Cancel"};
                            char action_choice[MAX_LEN];
                            if (display_list_and_get_selection("Action", (char**)actions, 3, action_choice) != -1) {
                                if (strcmp(action_choice, "Pair") == 0) {
                                    snprintf(command, sizeof(command), "echo -e 'pair %s\\nexit' | bluetoothctl", mac);
                                } else if (strcmp(action_choice, "Connect") == 0) {
                                    snprintf(command, sizeof(command), "echo -e 'connect %s\\nexit' | bluetoothctl", mac);
                                } else {
                                    command[0] = '\0';
                                }

                                if (strlen(command) > 0) {
                                    popup_message("Executing...", "Check terminal for PIN/passkey prompts.");
                                    system(command);
                                    popup_message("Info", "Action attempted. Check status.");
                                }
                            }
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                    case 3: // Disconnect
                        count = execute_command_and_parse("echo 'devices Paired' | bluetoothctl", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Disconnect a Device", lines, count, selected_item) != -1) {
                            char mac[18];
                            sscanf(selected_item, "%s", mac);
                            snprintf(command, sizeof(command), "echo -e 'disconnect %s\\nexit' | bluetoothctl", mac);
                            system(command);
                            popup_message("Bluetooth", "Disconnect command sent.");
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                }
                if (choice != n_choices - 1) choice = -1;
                break;
            case 'q':
            case 27:
                choice = n_choices - 1;
                break;
        }
    }
    delwin(bt_win);
}

void main_menu_loop() {
    const char *choices[] = {
        "1. Wi-Fi Manager",
        "2. Bluetooth Manager",
        "3. Help",
        "4. Exit"
    };
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0;
    int choice = 0;
    
    // ASCII Art
    const char *logo[] = {
        "  _   _      _   _             ",
        " | \\ | |    | | | |            ",
        " |  \\| | ___| |_| | __ _ _ __  ",
        " | . ` |/ _ \\ __| |/ _` | '_ \\ ",
        " | |\\  |  __/ |_| | (_| | | | |",
        " |_| \\_|\\___|\\__|_|\\__,_|_| |_|"
    };
    int logo_lines = sizeof(logo) / sizeof(char *);

    while (1) {
        clear();
        attron(A_BOLD | COLOR_PAIR(1));
        for (int i = 0; i < logo_lines; i++) {
            mvprintw(i + 2, (COLS - strlen(logo[i])) / 2, "%s", logo[i]);
        }
        attroff(A_BOLD | COLOR_PAIR(1));
        
        attron(A_DIM);
        mvprintw(logo_lines + 3, (COLS - 30) / 2, "A Vim-Style Network Manager");
        attroff(A_DIM);


        // Draw the menu box manually for the main screen
        int menu_h = n_choices + 2, menu_w = 30;
        int menu_y = logo_lines + 6, menu_x = (COLS - menu_w) / 2;
        WINDOW *menu_win = newwin(menu_h, menu_w, menu_y, menu_x);
        box(menu_win, 0, 0);

        for (int i = 0; i < n_choices; ++i) {
            if (highlight == i) {
                wattron(menu_win, COLOR_PAIR(2));
                mvwprintw(menu_win, i + 1, 2, "> %s", choices[i]);
                wattroff(menu_win, COLOR_PAIR(2));
            } else {
                mvwprintw(menu_win, i + 1, 2, "  %s", choices[i]);
            }
        }
        wrefresh(menu_win);
        refresh();

        int c = getch();
        switch (c) {
            case 'k':
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case 'j':
            case KEY_DOWN:
                if (highlight < n_choices - 1) highlight++;
                break;
            case '1': highlight = 0; choice = 10; break;
            case '2': highlight = 1; choice = 10; break;
            case '3': highlight = 2; choice = 10; break;
            case '4': highlight = 3; choice = 10; break;
            case 10: // Enter
                choice = 10;
                break;
            case 'q':
            case 27:
                highlight = n_choices - 1;
                choice = 10;
                break;
        }

        if (choice == 10) { // If an action was triggered
            switch (highlight) {
                case 0: wifi_manager_loop(); break;
                case 1: bluetooth_manager_loop(); break;
                case 2: popup_message("Help", "Navigate with j/k or arrows. Enter to select. q/ESC to go back. Run with sudo!"); break;
                case 3: 
                    delwin(menu_win);
                    return; // Exit
            }
            choice = 0; // Reset choice
        }
         delwin(menu_win);
    }
}

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        fprintf(stderr, "Error: This program requires superuser privileges. Please run with sudo.\n");
        return 1;
    }

    init_ncurses();
    main_menu_loop();
    cleanup_ncurses();

    return 0;
}
