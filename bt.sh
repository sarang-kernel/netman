#!/bin/bash

# Dependencies: bluetoothctl, awk, grep, sed, sleep

echo "[*] Starting bluetoothctl..."
bluetoothctl << EOF
power on
agent on
default-agent
EOF

echo "[*] Scanning for Bluetooth devices (5 seconds)..."
bluetoothctl scan on &

# Give time to discover devices
sleep 5

# Kill scan
bluetoothctl scan off > /dev/null

# Get list of discovered devices
DEVICE_LIST=$(bluetoothctl devices | grep -v "[CHG]" | awk '{$1=""; print $0}' | nl -w2 -s'. ')
NUM_DEVICES=$(echo "$DEVICE_LIST" | wc -l)

if [[ $NUM_DEVICES -eq 0 ]]; then
    echo "No Bluetooth devices found. Try again."
    exit 1
fi

echo
echo "Discovered Devices:"
echo "$DEVICE_LIST"
echo

# Prompt user to select device
read -p "Enter the number of the device you want to connect to: " SELECTION

# Get the selected MAC address
SELECTED_LINE=$(echo "$DEVICE_LIST" | sed -n "${SELECTION}p")
MAC=$(bluetoothctl devices | grep "$(echo "$SELECTED_LINE" | sed 's/^[0-9]*\. //')" | awk '{print $2}')

if [[ -z "$MAC" ]]; then
    echo "Invalid selection."
    exit 1
fi

echo "[*] Pairing with $MAC..."
bluetoothctl << EOF
pair $MAC
trust $MAC
connect $MAC
EOF

