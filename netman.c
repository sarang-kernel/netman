#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_ITEMS 50
#define MAX_LEN 256
#define CMD_BUF_SIZE 1024

// --- Function Prototypes ---
void init_ncurses();
void cleanup_ncurses();
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
    }
}

void cleanup_ncurses() {
    endwin();
}

/**
 * @brief Temporarily exits ncurses to run a command that might need the raw terminal (e.g., for PIN entry).
 */
void run_system_command_interactive(const char* command) {
    cleanup_ncurses();
    printf("\n--- Executing command, please follow prompts in terminal ---\n");
    system(command);
    printf("--- Command finished, press ENTER to return to the TUI ---\n");
    while (getchar() != '\n'); // Clear input buffer
    init_ncurses();
    refresh();
}


void draw_menu(WINDOW *win, const char *title, const char **choices, int n_choices, int highlight, int start_y, int start_x) {
    werase(win);
    box(win, 0, 0);
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 1, (getmaxx(win) - strlen(title)) / 2, "%s", title);
    wattroff(win, A_BOLD | COLOR_PAIR(1));
    wattron(win, A_DIM);
    mvwprintw(win, getmaxy(win) - 2, 2, "j/k/Arrows: Navigate | Enter: Select | q/ESC: Back");
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
    int height = 10, width = 70;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    WINDOW *popup = newwin(height, width, starty, startx);
    box(popup, 0, 0);
    wattron(popup, A_BOLD | COLOR_PAIR(1));
    mvwprintw(popup, 1, (width - strlen(title)) / 2, "%s", title);
    wattroff(popup, A_BOLD | COLOR_PAIR(1));
    mvwprintw(popup, 3, 2, "%.*s", width - 4, message);
    mvwprintw(popup, height - 3, 2, "Press any key to continue...");
    wrefresh(popup);
    wgetch(popup);
    delwin(popup);
    touchwin(stdscr);
    refresh();
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
    init_ncurses();
    refresh();
    return 0;
}

int execute_command_and_parse(const char *command, char **output_lines, int max_lines) {
    FILE *pipe = popen(command, "r");
    if (!pipe) { popup_message("Error", "Failed to execute command."); return 0; }
    char line[MAX_LEN];
    int count = 0;
    while (fgets(line, sizeof(line), pipe) != NULL && count < max_lines) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 1) {
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
    delwin(list_win);
    touchwin(stdscr);
    refresh();
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
                    case 0: // Scan
                        popup_message("Scanning...", "Scanning for Wi-Fi networks...");
                        system("iwctl station wlan0 scan > /dev/null 2>&1");
                        popup_message("Scan Complete", "Network scan finished.");
                        break;
                    case 1: // List & Connect
                        // This command now formats the output cleanly into three columns.
                        count = execute_command_and_parse("iwctl station wlan0 get-networks | tail -n +2 | awk '{ printf(\"%-25s %-15s %s\\n\", $1, $2, $3) }'", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Available Wi-Fi Networks (SSID | Security | Signal)", lines, count, selected_item) != -1) {
                            char ssid[MAX_LEN] = {0}, password[MAX_LEN] = {0};
                            sscanf(selected_item, "%s", ssid); // Extract just the SSID
                            get_input_from_dialog("Password Required (leave blank for open networks)", ssid, password, 1);
                            if (strlen(password) > 0) {
                                snprintf(command, sizeof(command), "iwctl station wlan0 connect \"%s\" --passphrase \"%s\"", ssid, password);
                            } else {
                                snprintf(command, sizeof(command), "iwctl station wlan0 connect \"%s\"", ssid);
                            }
                            if (system(command) == 0) popup_message("Success", "Connected successfully.");
                            else popup_message("Failure", "Failed to connect.");
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                    case 2: // Status
                        system("iwctl station wlan0 show > /tmp/netman_status");
                        FILE *f = fopen("/tmp/netman_status", "r");
                        if(f) { fread(output, 1, sizeof(output)-1, f); fclose(f); }
                        remove("/tmp/netman_status");
                        popup_message("Wi-Fi Status", strlen(output) > 0 ? output : "Not connected or device not found.");
                        break;
                    case 3: // Disconnect
                        system("iwctl station wlan0 disconnect");
                        popup_message("Wi-Fi", "Disconnected from network.");
                        break;
                    case 4: // Forget Network
                        count = execute_command_and_parse("iwctl known-networks list | tail -n +2 | awk '{print $1}'", lines, MAX_ITEMS);
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
    const char *choices[] = { "Power On/Off", "Scan for Devices", "List/Pair/Connect", "Disconnect", "Back" };
    int n_choices = sizeof(choices) / sizeof(char *);
    int highlight = 0, choice = -1;
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);

    while (choice != n_choices - 1) {
        draw_menu(win, "Bluetooth Manager", choices, n_choices, highlight, (LINES - n_choices) / 2, (COLS - 40) / 2);
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
                    case 0: // Power Toggle
                        if (system("echo -e 'show\nexit' | bluetoothctl | grep -q 'Powered: yes'") == 0) {
                            system("echo -e 'power off\nexit' | bluetoothctl > /dev/null");
                            popup_message("Bluetooth", "Powered OFF.");
                        } else {
                            system("echo -e 'power on\nexit' | bluetoothctl > /dev/null");
                            popup_message("Bluetooth", "Powered ON.");
                        }
                        break;
                    case 1: // Scan
                        popup_message("Scanning...", "Scanning for devices for 10s...");
                        // This is a cleaner way to scan without hanging or using pkill
                        system("(echo 'scan on' ; sleep 10 ; echo 'scan off') | bluetoothctl > /dev/null");
                        popup_message("Scan Complete", "Device scan finished.");
                        break;
                    case 2: // List/Pair/Connect
                        count = execute_command_and_parse("echo -e 'devices\nexit' | bluetoothctl | sed 's/Device //' | awk '{$1=$1;print}'", lines, MAX_ITEMS);
                        if (display_list_and_get_selection("Available Bluetooth Devices", lines, count, selected_item) != -1) {
                            char mac[18];
                            sscanf(selected_item, "%s", mac); // Extract MAC address
                            const char *actions[] = {"Pair", "Connect", "Cancel"};
                            char action_choice[MAX_LEN];
                            if (display_list_and_get_selection("Action", (char**)actions, 3, action_choice) != -1) {
                                if (strcmp(action_choice, "Pair") == 0) snprintf(command, sizeof(command), "echo -e 'pair %s\\nexit' | bluetoothctl", mac);
                                else if (strcmp(action_choice, "Connect") == 0) snprintf(command, sizeof(command), "echo -e 'connect %s\\nexit' | bluetoothctl", mac);
                                else command[0] = '\0';

                                if (strlen(command) > 0) {
                                    run_system_command_interactive(command);
                                    popup_message("Info", "Action attempted. Check device status.");
                                }
                            }
                        }
                        for (int i = 0; i < count; i++) free(lines[i]);
                        break;
                    case 3: // Disconnect
                        count = execute_command_and_parse("echo -e 'devices Connected\nexit' | bluetoothctl | sed 's/Device //' | awk '{$1=$1;print}'", lines, MAX_ITEMS);
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
    int menu_h = n_choices + 4, menu_w = 35;
    int menu_y = logo_lines + 5, menu_x = (COLS - menu_w) / 2;
    WINDOW *menu_win = newwin(menu_h, menu_w, menu_y, menu_x);
    keypad(menu_win, TRUE);

    while (1) {
        clear();
        attron(A_BOLD | COLOR_PAIR(1));
        for (int i = 0; i < logo_lines; i++) mvprintw(i + 2, (COLS - strlen(logo[i])) / 2, "%s", logo[i]);
        attroff(A_BOLD | COLOR_PAIR(1));
        attron(A_DIM);
        mvprintw(logo_lines + 3, (COLS - 30) / 2, "A Vim-Style Network Manager");
        attroff(A_DIM);
        refresh();
        draw_menu(menu_win, "", choices, n_choices, highlight, 2, 3);

        int c = wgetch(menu_win);
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
                case 3: delwin(menu_win); return;
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
