CC 		:= $(shell command -v gcc-15 2>/dev/null || echo gcc)
CFLAGS  := -std=c17 -Wall -Wextra -Wpedantic -g -O3 -march=native
LDFLAGS :=

SRC_DIR    := src
INC_DIR    := include
DEPS_DIR   := deps
BUILD_DIR  := build
TARGET     := cortex-vm

DEPS_SRCS := $(shell find $(DEPS_DIR) -type f -name '*.c' ! -path '*/testing/*' 2>/dev/null)
DEPS_INC_DIRS := $(sort $(dir $(shell find $(DEPS_DIR) -type f -name '*.h' 2>/dev/null)))
CPPFLAGS := -I$(INC_DIR) $(addprefix -I,$(DEPS_INC_DIRS))

SRCS := $(wildcard $(SRC_DIR)/*.c) main.c $(DEPS_SRCS)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(RUN_ARGS)

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DDEBUG" all

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
