# Using Cortex-VM as a Library

Cortex-VM can be embedded directly into another C project as a static library. This lets you assemble and execute Cortex assembly at runtime from within your own program — no subprocess, no CLI, no user-visible VM.

---

## Table of Contents

1. [Adding as a Git Submodule](#1-adding-as-a-git-submodule)
2. [Building the Library](#2-building-the-library)
3. [Integrating into Your Makefile](#3-integrating-into-your-makefile)
4. [API Reference](#4-api-reference)
5. [Examples](#5-examples)

---

## 1. Adding as a Git Submodule

```sh
git submodule add https://github.com/jonahmer22/cortex-vm.git deps/cortex-vm
git submodule update --init --recursive
```

The `--recursive` flag is required — cortex-vm has its own submodule dependencies (arena, cliargs) that must also be initialized.

When someone clones your project for the first time:

```sh
git clone --recurse-submodules https://github.com/you/your-project.git
```

Or if they already cloned without `--recurse-submodules`:

```sh
git submodule update --init --recursive
```

---

## 2. Building the Library

From the cortex-vm directory (or driven by your Makefile):

```sh
make -C deps/cortex-vm lib
```

This produces:

```
deps/cortex-vm/lib/
  libcortex-vm.a    # static library
  cortex-vm.h       # public API header
```

---

## 3. Integrating into Your Makefile

```makefile
CORTEX_DIR := deps/cortex-vm
CORTEX_LIB := $(CORTEX_DIR)/lib/libcortex-vm.a
CORTEX_H   := $(CORTEX_DIR)/lib

# build the library before your project if it doesn't exist
$(CORTEX_LIB):
	git submodule update --init --recursive
	$(MAKE) -C $(CORTEX_DIR) lib

CPPFLAGS += -I$(CORTEX_H)
LDFLAGS  += $(CORTEX_LIB) -lm

# make your build depend on the library
your-binary: $(CORTEX_LIB) $(YOUR_OBJS)
	$(CC) $(YOUR_OBJS) $(LDFLAGS) -o $@
```

---

## 4. API Reference

Include the header:

```c
#include "cortex-vm.h"
```

### `cortexAssemble`

```c
uint64_t *cortexAssemble(const char *source, const char *outputPath);
```

Assembles the given source string and writes the binary to `outputPath`. If `outputPath` is `NULL`, the binary is written to `"a.out"`.

Returns a heap-allocated word buffer containing the assembled binary. `binary[1]` holds the word count. The caller is responsible for freeing the returned pointer.

Use this when you need the binary in memory (to run, cache, or inspect it) and also want it written to disk.

### `cortexExecSource`

```c
int cortexExecSource(const char *source);
```

Assembles the given source string and immediately executes it. Returns the program's exit code.

The source string is not modified. You do not need to manage any buffers — assembly, execution, and cleanup are all handled internally.

### `cortexExecBinary`

```c
int cortexExecBinary(const uint64_t *binary, size_t wordCount);
```

Executes a pre-assembled binary given as a word buffer. Returns the program's exit code.

`binary` must point to a valid Cortex-VM binary (5-word header followed by instructions and optional data). The caller is responsible for freeing `binary` after this call returns.

---

## 5. Examples

### Run assembly from a string

```c
#include "cortex-vm.h"

int main(void) {
    const char *src =
        "main:\n"
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"   // SYS_PRINT_STR
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"   // SYS_EXIT
        "    syscall\n"
        ".data\n"
        "    msg: \"Hello from embedded Cortex-VM!\\n\"\n";

    return cortexExecSource(src);
}
```

### Compile to disk and run

```c
#include <stdint.h>
#include <stdio.h>
#include "cortex-vm.h"

int main(void) {
    const char *src =
        "main:\n"
        "    addi a0, zero, 42\n"
        "    addi a13, zero, 0\n"   // SYS_EXIT with code 42
        "    syscall\n";

    // assemble and write to disk; also get the binary in memory
    uint64_t *binary = cortexAssemble(src, "output.out");

    // run from the in-memory buffer — no second disk read needed
    int code = cortexExecBinary(binary, binary[1]);
    printf("program exited with code %d\n", code);   // 42

    free(binary);
    return 0;
}
```

### Compile once, run multiple times

```c
#include <stdlib.h>
#include "cortex-vm.h"

int main(void) {
    const char *src =
        "main:\n"
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"hello\\n\"\n";

    uint64_t *binary = cortexAssemble(src, NULL);   // writes to a.out

    cortexExecBinary(binary, binary[1]);
    cortexExecBinary(binary, binary[1]);   // second run — no re-assembly

    free(binary);
    return 0;
}
```

### Loading assembly from a file

```c
#include <stdio.h>
#include <stdlib.h>
#include "cortex-vm.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)len + 1);
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char *src = read_file(argv[1]);
    if (!src) return 1;
    int code = cortexExecSource(src);
    free(src);
    return code;
}
```

---

## Notes

- **Thread safety:** The assembler uses global state. Do not call `cortexExecSource` or `cortexExecBinary` concurrently from multiple threads.
- **Exit calls:** `SYS_EXIT` in the Cortex program returns the exit code to your caller — it does not call the C `exit()` function, so your process continues normally after `cortexExecSource` returns.
- **Error output:** Assembly and header validation errors are printed to `stderr` and call `exit()`. If you need softer error handling, this is a known limitation of the current API.
