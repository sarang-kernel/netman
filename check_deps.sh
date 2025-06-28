#!/bin/bash

# --- NetMan TUI Dependency Checker ---

BOLD=$(tput bold); GREEN=$(tput setaf 2); YELLOW=$(tput setaf 3); RED=$(tput setaf 1); RESET=$(tput sgr0)
echo "${BOLD}Checking dependencies for NetMan TUI...${RESET}"

# rfkill is now used for stable power toggling
REQUIRED_BINS=("gcc" "make" "dialog" "iwctl" "bluetoothctl" "rfkill")

declare -A PKG_MAP
PKG_MAP=(
    [gcc]="build-essential gcc"
    [make]="build-essential make"
    [dialog]="dialog"
    [iwctl]="iwd"
    [bluetoothctl]="bluez"
    [rfkill]="rfkill"
    # Pacman (Arch Linux)
    [pacman_bluetoothctl]="bluez-utils"
    [pacman_rfkill]="util-linux" # rfkill is in util-linux on Arch
    # DNF (Fedora) / Zypper (openSUSE)
    [dnf_bluetoothctl]="bluez bluez-tools"
    [zypper_bluetoothctl]="bluez bluez-tools"
)

if command -v apt-get &> /dev/null; then PM="apt"; PM_INSTALL_CMD="sudo apt-get install -y";
elif command -v pacman &> /dev/null; then PM="pacman"; PM_INSTALL_CMD="sudo pacman -S --noconfirm";
elif command -v dnf &> /dev/null; then PM="dnf"; PM_INSTALL_CMD="sudo dnf install -y";
elif command -v zypper &> /dev/null; then PM="zypper"; PM_INSTALL_CMD="sudo zypper install -y";
else echo "${RED}Unsupported package manager. Please install dependencies manually.${RESET}"; exit 1; fi
echo "Detected package manager: ${GREEN}$PM${RESET}"

MISSING_PKGS=()
if ! pkg-config --exists ncursesw; then
    case "$PM" in
        apt) MISSING_PKGS+=("libncurses-dev");;
        pacman) MISSING_PKGS+=("ncurses");;
        *) MISSING_PKGS+=("ncurses-devel");;
    esac
fi

for bin in "${REQUIRED_BINS[@]}"; do
    if ! command -v "$bin" &> /dev/null; then
        echo "${YELLOW}Dependency missing: $bin${RESET}"
        key="$bin"
        pm_specific_key="${PM}_${key}"
        if [[ -n "${PKG_MAP[$pm_specific_key]}" ]]; then
            MISSING_PKGS+=(${PKG_MAP[$pm_specific_key]})
        else
            MISSING_PKGS+=(${PKG_MAP[$key]})
        fi
    else
        echo "${GREEN}Found: $bin${RESET}"
    fi
done

if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
    UNIQUE_PKGS=($(echo "${MISSING_PKGS[@]}" | tr ' ' '\n' | sort -u | tr '\n' ' '))
    echo; echo "${YELLOW}The following packages are missing: ${BOLD}${UNIQUE_PKGS[*]}${RESET}"
    read -p "Do you want to install them now? (y/N) " -n 1 -r; echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing with command: ${BOLD}$PM_INSTALL_CMD ${UNIQUE_PKGS[*]}${RESET}"
        $PM_INSTALL_CMD ${UNIQUE_PKGS[*]}
        if [ $? -ne 0 ]; then echo "${RED}Error: Failed to install dependencies.${RESET}"; exit 1; fi
    else echo "${RED}Installation cancelled.${RESET}"; exit 1; fi
else echo; echo "${GREEN}${BOLD}All dependencies are satisfied.${RESET}"; fi

exit 0
