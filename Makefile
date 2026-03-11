CC      := gcc
CFLAGS  := -std=c17 -Wall -Wextra -Wpedantic -g
LDFLAGS :=

SRC_DIR    := src
INC_DIR    := include
BUILD_DIR  := build
TARGET     := cortex-vm

SRCS := $(wildcard $(SRC_DIR)/*.c) main.c
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(RUN_ARGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
