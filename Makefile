CC 		:= $(shell command -v gcc-15 2>/dev/null || echo gcc)
CFLAGS  := -std=c17 -Wall -Wextra -Wpedantic -g -O3 -march=native
LDFLAGS := -lm

SRC_DIR    := src
INC_DIR    := include
DEPS_DIR   := deps
BUILD_DIR  := build
TARGET     := cortex-vm

DEPS_SRCS := $(shell find $(DEPS_DIR) -type f -name '*.c' ! -path '*/testing/*' 2>/dev/null)
DEPS_INC_DIRS := $(sort $(dir $(shell find $(DEPS_DIR) -type f -name '*.h' 2>/dev/null)))
CPPFLAGS := -I$(INC_DIR) $(addprefix -I,$(DEPS_INC_DIRS))

LIB_DIR    := lib
LIB_TARGET := $(LIB_DIR)/libcortex-vm.a
LIB_SRCS   := $(wildcard $(SRC_DIR)/*.c) $(DEPS_SRCS)
LIB_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))

SRCS := $(wildcard $(SRC_DIR)/*.c) main.c $(DEPS_SRCS)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all lib clean run debug

all: $(TARGET)

lib: $(LIB_TARGET)

$(LIB_TARGET): $(LIB_OBJS)
	@mkdir -p $(LIB_DIR)
	ar rcs $@ $^
	cp $(INC_DIR)/cortex-vm.h $(LIB_DIR)/cortex-vm.h

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
	rm -rf $(BUILD_DIR) $(TARGET) $(LIB_DIR)

-include $(DEPS)
