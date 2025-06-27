# Makefile for NetMan TUI

# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -lncurses
LDFLAGS =

# The target executable
TARGET = netman

# Source files
SRC = netman.c

# Phony targets
.PHONY: all clean install check_deps

# Default target
all: $(TARGET)

# Link the object file into the final executable
$(TARGET): $(SRC)
	@echo "Compiling $(TARGET)..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "Compilation complete. Run with 'sudo ./$(TARGET)'"

# Check dependencies and then build
install: check_deps all
	@echo ""
	@echo "Installation process finished."
	@echo "To run the application, use: sudo ./$(TARGET)"

# Run the dependency checker script
check_deps: check_deps.sh
	@chmod +x check_deps.sh
	@./check_deps.sh

# Clean up build artifacts
clean:
	@echo "Cleaning up..."
	rm -f $(TARGET)
