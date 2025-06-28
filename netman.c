#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#define MAX_ITEMS 50
#define MAX_LEN 256
#define CMD_BUF_SIZE 1024

// --- Function Prototypes ---
void init_ncurses();
void cleanup_ncurses();
void sanitize_ansi_codes(char *str);
void run_command_silent(const char* command);
void show_loading_animation(const char* command, const char* message, int timeout_sec);
void run_system_command_interactive(const char* command);
void draw_menu(WINDOW *win, const char *title, const char **choices, int n_choices, int highlight, int start_y, int start_x);
void popup_message(const char *title, const char *message);
int get_input_from_dialog(const char *title, const char *prompt, char *result, int is_password);
int execute_command_and_parse(const char *command, char **output_lines, int max_lines);
int display_list_and_get_selection(const char *title, char **items, int count, char *selected_item);
void main_menu_loop();
void wifi_manager_loop();
void bluetooth_manager_loop();

// --- UI & System Interaction ---

void init_ncurses() {
    initscr(); clear(); noecho(); cbreak(); curs_set(0); keypad(stdscr, TRUE);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_CYAN);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    }
}

void cleanup_ncurses() { endwin(); }

void sanitize_ansi_codes(char *str) {
    char *p = str, *q = str;
    while (*p) {
        if (*p == '\x1B') {
            p++; if (*p == '[') { p++; while (isdigit(*p) || *p == ';') p++; if (*p == 'm') p++; }
        } else { *q++ = *p++; }
    }
    *q = '\0';
}

void run_command_silent(const char* command) {
    pid_t pid = fork();
    if (pid == -1) { return; }
    if (pid == 0) {
        freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "sh", "-c", command, (char *) NULL);
        _exit(127);
    } else {
        waitpid(pid, NULL, 0);
    }
}

void show_loading_animation(const char* command, const char* message, int timeout_sec) {
    WINDOW *win = newwin(7, 50, (LINES - 7) / 2, (COLS - 50) / 2);
    box(win, 0, 0);
    pid_t pid = fork();
    if (pid == -1) { popup_message("Error", "Failed to fork process."); return; }

    if (pid == 0) {
        freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "sh", "-c", command, (char *) NULL);
        _exit(127);
    } else {
        const char *spinner = "|/-\\"; int i = 0;
        nodelay(win, TRUE);
        for(int t = 0; t < timeout_sec * 10; t++) {
            if (waitpid(pid, NULL, WNOHANG) != 0) break;
            mvwprintw(win, 2, 3, "%s", message);
            mvwprintw(win, 4, 24, "%c", spinner[i++ % 4]);
            wrefresh(win);
            usleep(100000);
        }
        if (waitpid(pid, NULL, WNOHANG) == 0) { kill(pid, SIGTERM); waitpid(pid, NULL, 0); }
    }
    delwin(win); touchwin(stdscr); refresh();
}

void run_system_command_interactive(const char* command) {
    cleanup_ncurses();
    printf("\n--- Executing command, please follow prompts in terminal ---\n\n");
    system(command);
    printf("\n--- Command finished, press ENTER to return to the TUI ---\n");
    while (getchar() != '\n' && getchar() != EOF);
    init_ncurses(); refresh();
}

void draw_menu(WINDOW *win, const char *title, const char **choices, int n_choices, int highlight, int start_y, int start_x) {
    werase(win); box(win, 0, 0);
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 1, (getmaxx(win) - strlen(title)) / 2, "%s", title);
    wattroff(win, A_BOLD | COLOR_PAIR(1));
    wattron(win, A_DIM);
    mvwprintw(win, getmaxy(win) - 2, 2, "j/k/Arrows: Nav | Enter: Select | q/ESC: Back");
    wattroff(win, A_DIM);
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
    char clean_message[4096];
    strncpy(clean_message, message, sizeof(clean_message) - 1);
    clean_message[sizeof(clean_message) - 1] = '\0';
    sanitize_ansi_codes(clean_message);

    int height = 12, width = 70;
    WINDOW *popup = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);
    box(popup, 0, 0);
    wattron(popup, A_BOLD | COLOR_PAIR(1));
    mvwprintw(popup, 1, (width - strlen(title)) / 2, "%s", title);
    wattroff(popup, A_BOLD | COLOR_PAIR(1));
    mvwprintw(popup, 3, 2, "%.*s", width - 4, clean_message);
    mvwprintw(popup, height - 3, 2, "Press any key to continue...");
    wrefresh(popup); wgetch(popup);
    delwin(popup); touchwin(stdscr); refresh();
}

int get_input_from_dialog(const char *title, const char *prompt, char *result, int is_password) {
    char command[CMD_BUF_SIZE];
    const char* dialog_type = is_password ? "--passwordbox" : "--inputbox";
    snprintf(command, sizeof(command), "dialog --stdout --title \"%s\" %s \"%s\" 10 60", title, dialog_type, prompt);
    cleanup_ncurses();
    FILE *pipe = popen(command, "r");
    if (!pipe) { init_ncurses(); return -1; }
    if (fgets(result, MAX_LEN, pipe) == NULL) { result[0] = '\0'; }
    else { result[strcspn(result, "\n")] = 0; }
    pclose(pipe);
    init_ncurses(); refresh();
    return 0;
}

int execute_command_and_parse(const char *command, char **output_lines, int max_lines) {
    FILE *pipe = popen(command, "r");
    if (!pipe) { popup_message("Error", "Failed to execute command."); return 0; }
    char line[MAX_LEN];
    int count = 0;
    while (fgets(line, sizeof(line), pipe) != NULL && count < max_lines) {
        line[strcspn(line, "\n")] = 0;
        sanitize_ansi_codes(line);
        if (strlen(line) > 1 && !strstr(line, "---") && !strstr(line, "Searching")) {
            output_lines[count] = strdup(line);
            count++;
        }
    }
    pclose(pipe);
    return count;
}

int display_list_and_get_selection(const char *title, char **items, int count, char *selected_item) {
    if (count == 0) { popup_message(title, "No items found."); return -1; }
    WINDOW *list_win = newwin(LINES - 4, COLS - 4, 2, 2);
    keypad(list_win, TRUE);
    int highlight = 0, choice = -2;
    while (1) {
        draw_menu(list_win, title, (const char **)items, count, highlight, 3, 4);
        int c = wgetch(list_win);
        switch (c) {
            case 'k': case KEY_UP: if (highlight > 0) highlight--; break;
            case 'j': case KEY_DOWN: if (highlight < count - 1) highlight++; break;
            case 10: choice = highlight; break;
            case 'q': case 27: choice = -1; break;
        }
        if (choice != -2) break;
    }
    if (choice != -1) { strncpy(selected_item, items[choice], MAX_LEN - 1); selected_item[MAX_LEN - 1] = '\0'; }
    delwin(list_win); touchwin(stdscr); refresh();
    return choice;
}

// --- Manager Loops ---

void wifi_manager_loop() {
    const char *choices[] = { "Scan for Networks", "List & Connect", "Show Status", "Disconnect", "Forget a Network", "Back" };
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0, choice = -1;
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);

    while (choice != n_choices - 1) {
        draw_menu(win, "Wi-Fi Manager", choices, n_choices, highlight, (LINES - n_choices) / 2, (COLS - 40) / 2);
        int c = wgetch(win);
        switch (c) {
            case 'k': case KEY_UP: if (highlight > 0) highlight--; break;
            case 'j': case KEY_DOWN: if (highlight < n_choices - 1) highlight++; break;
            case 10:
                choice = highlight;
                char command[CMD_BUF_SIZE], output[4096] = {0}, selected_item[MAX_LEN];
                char *lines[MAX_ITEMS];
                int count;

                switch (choice) {
                    case 0:
                        show_loading_animation("iwctl station wlan0 scan", "Scanning for Wi-Fi networks...", 10);
                        popup_message("Scan Complete", "Network scan finished.");
                        break;
                    case 1:
                        count = execute_command_and_parse("iwctl station wlan0 get-networks | awk 'NF >= 3 && $1 != \">\" { printf(\"%-25s %-15s %s\\n\", $1, $2, $3) }'", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Available Wi-Fi Networks (SSID | Security | Signal)", lines, count, selected_item) != -1) {
                            char ssid[MAX_LEN] = {0}, password[MAX_LEN] = {0};
                            sscanf(selected_item, "%s", ssid);
                            get_input_from_dialog("Password Required (leave blank for open networks)", ssid, password, 1);
                            if (strlen(password) > 0) snprintf(command, sizeof(command), "iwctl station wlan0 connect \"%s\" --passphrase \"%s\"", ssid, password);
                            else snprintf(command, sizeof(command), "iwctl station wlan0 connect \"%s\"", ssid);
                            if (system(command) == 0) popup_message("Success", "Connected successfully.");
                            else popup_message("Failure", "Failed to connect.");
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                    case 2:
                        {
                            FILE *pipe = popen("iwctl station wlan0 show", "r");
                            if(pipe) { fread(output, 1, sizeof(output)-1, pipe); pclose(pipe); }
                            popup_message("Wi-Fi Status", strlen(output) > 0 ? output : "Not connected or device not found.");
                        }
                        break;
                    case 3: system("iwctl station wlan0 disconnect"); popup_message("Wi-Fi", "Disconnected from network."); break;
                    case 4:
                        count = execute_command_and_parse("iwctl known-networks list | awk 'NF >= 1 && $1 != \">\" {print $1}'", lines, MAX_ITEMS);
                         if (display_list_and_get_selection("Forget a Network", lines, count, selected_item) != -1) {
                            snprintf(command, sizeof(command), "iwctl known-networks %s forget", selected_item);
                            if(system(command) == 0) popup_message("Success", "Network forgotten.");
                            else popup_message("Error", "Could not forget network.");
                         }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                }
                if (choice != n_choices - 1) choice = -1; 
                break;
            case 'q': case 27: choice = n_choices - 1; break;
        }
    }
    delwin(win);
}

void bluetooth_manager_loop() {
    const char *choices[] = { "Power On/Off", "Scan, List & Connect", "Disconnect", "Back" };
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0, choice = -1;
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);

    while (choice != n_choices - 1) {
        draw_menu(win, "Bluetooth Manager", choices, n_choices, highlight, (LINES - n_choices) / 2, (COLS - 50) / 2);
        int c = wgetch(win);
        switch (c) {
            case 'k': case KEY_UP: if (highlight > 0) highlight--; break;
            case 'j': case KEY_DOWN: if (highlight < n_choices - 1) highlight++; break;
            case 10:
                choice = highlight;
                char command[CMD_BUF_SIZE], selected_item[MAX_LEN];
                char *lines[MAX_ITEMS];
                int count;

                switch (choice) {
                    case 0: // Power Toggle using rfkill
                        if (system("rfkill list bluetooth | grep -q 'Soft blocked: yes'") == 0) {
                            run_command_silent("rfkill unblock bluetooth");
                            popup_message("Bluetooth", "Powered ON.");
                        } else {
                            run_command_silent("rfkill block bluetooth");
                            popup_message("Bluetooth", "Powered OFF.");
                        }
                        break;
                    case 1: // Scan, List & Connect
                        show_loading_animation("bluetoothctl --timeout 10 scan on", "Scanning for Bluetooth devices...", 10);
                        count = execute_command_and_parse("echo -e 'devices\\nexit' | bluetoothctl", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Available Bluetooth Devices (MAC | Name)", lines, count, selected_item) != -1) {
                            char mac[18];
                            sscanf(selected_item, "%*s %s", mac);
                            const char *actions[] = {"Pair", "Connect", "Cancel"};
                            char action_choice[MAX_LEN];
                            if (display_list_and_get_selection("Action", (char**)actions, 3, action_choice) != -1) {
                                if (strcmp(action_choice, "Pair") == 0) snprintf(command, sizeof(command), "bluetoothctl pair %s", mac);
                                else if (strcmp(action_choice, "Connect") == 0) snprintf(command, sizeof(command), "bluetoothctl connect %s", mac);
                                else command[0] = '\0';

                                if (strlen(command) > 0) {
                                    run_system_command_interactive(command);
                                    popup_message("Info", "Action attempted. Check device status.");
                                }
                            }
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                    case 2: // Disconnect
                        count = execute_command_and_parse("echo -e 'devices Connected\\nexit' | bluetoothctl", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Disconnect a Device (MAC | Name)", lines, count, selected_item) != -1) {
                            char mac[18];
                            sscanf(selected_item, "%*s %s", mac);
                            snprintf(command, sizeof(command), "echo -e 'disconnect %s\\nexit' | bluetoothctl", mac);
                            run_command_silent(command);
                            popup_message("Bluetooth", "Disconnect command sent.");
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                }
                if (choice != n_choices - 1) choice = -1;
                break;
            case 'q': case 27: choice = n_choices - 1; break;
        }
    }
    delwin(win);
}

void main_menu_loop() {
    const char *choices[] = {"Wi-Fi Manager", "Bluetooth Manager", "Help", "Exit"};
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0;
    const char *logo[] = { "  _   _      _   _             ", " | \\ | |    | | | |            ", " |  \\| | ___| |_| | __ _ _ __  ", " | . ` |/ _ \\ __| |/ _` | '_ \\ ", " | |\\  |  __/ |_| | (_| | | | |", " |_| \\_|\\___|\\__|_|\\__,_|_| |_|" };
    int logo_lines = sizeof(logo) / sizeof(char *);
    
    WINDOW *main_win = newwin(LINES, COLS, 0, 0);
    keypad(main_win, TRUE);

    while (1) {
        werase(main_win); box(main_win, 0, 0);
        wattron(main_win, A_BOLD | COLOR_PAIR(3));
        for (int i = 0; i < logo_lines; i++) mvwprintw(main_win, i + 2, (COLS - strlen(logo[i])) / 2, "%s", logo[i]);
        wattroff(main_win, A_BOLD | COLOR_PAIR(3));
        wattron(main_win, A_DIM);
        mvwprintw(main_win, logo_lines + 3, (COLS - 30) / 2, "A Vim-Style Network Manager");
        wattroff(main_win, A_DIM);
        
        int menu_y = logo_lines + 6;
        for (int i = 0; i < n_choices; ++i) {
            if (highlight == i) {
                wattron(main_win, COLOR_PAIR(2));
                mvwprintw(main_win, menu_y + i, (COLS - strlen(choices[i])) / 2, " > %s < ", choices[i]);
                wattroff(main_win, COLOR_PAIR(2));
            } else {
                mvwprintw(main_win, menu_y + i, (COLS - strlen(choices[i])) / 2, "   %s   ", choices[i]);
            }
        }
        wrefresh(main_win);

        int c = wgetch(main_win);
        int choice = -1;
        switch (c) {
            case 'k': case KEY_UP: if (highlight > 0) highlight--; break;
            case 'j': case KEY_DOWN: if (highlight < n_choices - 1) highlight++; break;
            case '1': highlight = 0; choice = 10; break;
            case '2': highlight = 1; choice = 10; break;
            case '3': highlight = 2; choice = 10; break;
            case '4': case 'q': case 27: highlight = 3; choice = 10; break;
            case 10: choice = 10; break;
        }
        if (choice == 10) {
            switch (highlight) {
                case 0: wifi_manager_loop(); break;
                case 1: bluetooth_manager_loop(); break;
                case 2: popup_message("Help", "Navigate with j/k or arrows. Enter to select. q/ESC to go back. Run with sudo!"); break;
                case 3: delwin(main_win); return;
            }
        }
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
