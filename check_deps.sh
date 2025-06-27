#!/bin/bash

# --- NetMan TUI Dependency Checker ---

# Styling
BOLD=$(tput bold)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
RED=$(tput setaf 1)
RESET=$(tput sgr0)

echo "${BOLD}Checking dependencies for NetMan TUI...${RESET}"

# List of required command-line tools
REQUIRED_BINS=("gcc" "make" "dialog" "iwctl" "bluetoothctl")
# Corresponding package names for different package managers
declare -A PKG_MAP
PKG_MAP=(
    [gcc]="build-essential gcc"
    [make]="build-essential make"
    [dialog]="dialog"
    [iwctl]="iwd"
    [bluetoothctl]="bluetooth bluez"
    # Pacman specific names
    [pacman_gcc]="base-devel gcc"
    [pacman_make]="base-devel make"
    [pacman_dialog]="dialog"
    [pacman_iwctl]="iwd"
    [pacman_bluetoothctl]="bluez bluez-utils"
    # DNF/Zypper specific names
    [dnf_gcc]="gcc"
    [dnf_make]="make"
    [dnf_dialog]="dialog"
    [dnf_iwctl]="iwd"
    [dnf_bluetoothctl]="bluez"
)

# Detect package manager
if command -v apt-get &> /dev/null; then
    PM="apt"
    PM_INSTALL_CMD="sudo apt-get install -y"
elif command -v pacman &> /dev/null; then
    PM="pacman"
    PM_INSTALL_CMD="sudo pacman -S --noconfirm"
elif command -v dnf &> /dev/null; then
    PM="dnf"
    PM_INSTALL_CMD="sudo dnf install -y"
elif command -v zypper &> /dev/null; then
    PM="zypper"
    PM_INSTALL_CMD="sudo zypper install -y"
else
    echo "${RED}Unsupported package manager. Please install dependencies manually.${RESET}"
    exit 1
fi
echo "Detected package manager: ${GREEN}$PM${RESET}"

# Check for ncurses development library
NCURSES_FOUND=false
if pkg-config --exists ncurses; then
    NCURSES_FOUND=true
elif [ -f /usr/include/ncurses.h ]; then
    NCURSES_FOUND=true
fi

if ! $NCURSES_FOUND; then
    case "$PM" in
        apt) MISSING_PKGS+=("libncurses-dev") ;;
        pacman) MISSING_PKGS+=("ncurses") ;;
        dnf) MISSING_PKGS+=("ncurses-devel") ;;
        zypper) MISSING_PKGS+=("ncurses-devel") ;;
    esac
fi


# Find missing binaries
MISSING_PKGS=()
for bin in "${REQUIRED_BINS[@]}"; do
    if ! command -v "$bin" &> /dev/null; then
        echo "${YELLOW}Dependency missing: $bin${RESET}"
        key="$bin"
        # Use PM-specific package names if they exist
        if [ "$PM" == "pacman" ] && [ -n "${PKG_MAP[pacman_$key]}" ]; then
            key="pacman_$key"
        elif [ "$PM" == "dnf" ] || [ "$PM" == "zypper" ]; then
            key="dnf_$key"
        fi
        MISSING_PKGS+=(${PKG_MAP[$key]})
    else
        echo "${GREEN}Found: $bin${RESET}"
    fi
done

# Install missing packages if any
if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
    # Remove duplicates
    UNIQUE_PKGS=($(echo "${MISSING_PKGS[@]}" | tr ' ' '\n' | sort -u | tr '\n' ' '))
    echo
    echo "${YELLOW}The following packages are missing: ${BOLD}${UNIQUE_PKGS[*]}${RESET}"
    read -p "Do you want to install them now? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing packages with command: ${BOLD}$PM_INSTALL_CMD ${UNIQUE_PKGS[*]}${RESET}"
        $PM_INSTALL_CMD ${UNIQUE_PKGS[*]}
        if [ $? -ne 0 ]; then
            echo "${RED}Error: Failed to install dependencies. Please install them manually and try again.${RESET}"
            exit 1
        fi
    else
        echo "${RED}Installation cancelled. Please install dependencies manually.${RESET}"
        exit 1
    fi
else
    echo
    echo "${GREEN}${BOLD}All dependencies are satisfied.${RESET}"
fi

exit 0
