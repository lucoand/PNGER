# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -Isrc
LDFLAGS = -lX11 -lz -lm

# Directories
SRC_DIR = src
BUILD_DIR = build

# File lists
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
TARGET = $(BUILD_DIR)/pnger

#default target
all: $(TARGET)

# Link target
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

# Compile source files to build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

