CC 		:= $(shell command -v gcc-15 2>/dev/null || echo gcc)
CFLAGS  := -std=c17 -Wall -Wextra -Wpedantic -Wunused-result -g -O3 -march=native
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

UI_HTML_H    := include/ui_html.h
FAVICON_H    := include/favicon_png.h
CM_CSS_H     := include/cm_css.h
CM_THEME_H   := include/cm_theme_css.h
CM_JS_H      := include/cm_js.h
CM_GAS_JS_H  := include/cm_gas_js.h

GENERATED_HEADERS := $(UI_HTML_H) $(FAVICON_H) $(CM_CSS_H) $(CM_THEME_H) $(CM_JS_H) $(CM_GAS_JS_H)

.PHONY: all lib clean run debug

all: $(GENERATED_HEADERS) $(TARGET)

lib: $(LIB_TARGET)

$(LIB_TARGET): $(LIB_OBJS)
	@mkdir -p $(LIB_DIR)
	ar rcs $@ $^
	cp $(INC_DIR)/cortex-vm.h $(LIB_DIR)/cortex-vm.h

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(RUN_ARGS)

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DDEBUG" all

$(UI_HTML_H): ui/index.html
	python3 -c "d=open('ui/index.html','rb').read();print('static const unsigned char UI_HTML[]={'+','.join(str(b)for b in d)+',0};');print(f'static const size_t UI_HTML_LEN={len(d)};')" > $@

$(FAVICON_H): cortex-logos/sq_blk.png
	python3 -c "d=open('cortex-logos/sq_blk.png','rb').read();print('static const unsigned char FAVICON_PNG[]={'+','.join(str(b)for b in d)+'};');print(f'static const size_t FAVICON_PNG_LEN={len(d)};')" > $@

$(CM_CSS_H): ui/vendor/codemirror.min.css
	python3 -c "d=open('ui/vendor/codemirror.min.css','rb').read();print('static const unsigned char CM_CSS[]={'+','.join(str(b)for b in d)+',0};');print(f'static const size_t CM_CSS_LEN={len(d)};')" > $@

$(CM_THEME_H): ui/vendor/material-darker.min.css
	python3 -c "d=open('ui/vendor/material-darker.min.css','rb').read();print('static const unsigned char CM_THEME_CSS[]={'+','.join(str(b)for b in d)+',0};');print(f'static const size_t CM_THEME_CSS_LEN={len(d)};')" > $@

$(CM_JS_H): ui/vendor/codemirror.min.js
	python3 -c "d=open('ui/vendor/codemirror.min.js','rb').read();print('static const unsigned char CM_JS[]={'+','.join(str(b)for b in d)+',0};');print(f'static const size_t CM_JS_LEN={len(d)};')" > $@

$(CM_GAS_JS_H): ui/vendor/gas.js
	python3 -c "d=open('ui/vendor/gas.js','rb').read();print('static const unsigned char CM_GAS_JS[]={'+','.join(str(b)for b in d)+',0};');print(f'static const size_t CM_GAS_JS_LEN={len(d)};')" > $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(LIB_DIR) $(GENERATED_HEADERS)

-include $(DEPS)
