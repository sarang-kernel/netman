# NetMan TUI - A Vim-Style Network Manager

NetMan TUI is a terminal user interface for managing Wi-Fi and Bluetooth connections using `iwctl` and `bluetoothctl`. It provides a simple, Vim-style navigable interface built with C and `ncurses`.



## 📁 Features

### 📡 Wi-Fi Management (`iwctl`)
- Scan for networks
- List and select available networks to connect
- Show current connection status
- Disconnect from the current network
- Forget a saved network

### 🔵 Bluetooth Management (`bluetoothctl`)
- Power Bluetooth on/off
- Scan for nearby devices
- List available/paired devices
- Pair, connect, and disconnect from devices by selecting from a list

### 📟 UI/UX
- LazyVim-style startup menu.
- Vim keybindings (`j`, `k`, `Enter`, `q`/`ESC`) for navigation.
- Pop-up dialogs for password input using `dialog`.
- Status and error feedback messages.

## 📦 Prerequisites

This tool is designed for Linux systems with `iwd` (`iwctl`) and `bluez` (`bluetoothctl`). It requires the following tools to be installed:

- `gcc` and `make` for compilation.
- `ncurses` library (`libncurses-dev` or `ncurses-devel`).
- `dialog` for pop-up boxes.
- `iwctl` (from `iwd` package).
- `bluetoothctl` (from `bluez` or `bluez-utils` package).

## 🚀 Installation & Usage

The project includes a `Makefile` and a dependency-checking script that simplifies installation.

**Permissions:** This tool requires `sudo` privileges for network and device operations. You must run the final executable with `sudo`.

### Step 1: Clone the Repository

Clone or download the project files (`netman.c`, `Makefile`, `check_deps.sh`) into a single directory.

```bash
git clone <repository_url>
cd netman-tui
```

### Step 2: Run the Installer 

The `make install` command will first run a script to check for missing dependencies. It will detect your package manager (`apt, pacman, dnf, zypper`) and prompt you to install any missing packages.

```bash
make install
```

This will : 
1. Check for `gcc, make, dialog, iwctl`, and `bluetoothctl`.
2. Ask for your `sudo` password to install any missing dependencies.
3. Compile the `netman.c` source code into an executable named `netman`.

### Step 3: Run the Application

Run the compiled program with `sudo`.

```bash
sudo ./netman
```

Navigation: 
* `j/k` or `Arrow Down/Up`: Move selection.
* `Enter`: Select an option or action.
* `q` or `ESC`: Go back to previous menu or exit.
