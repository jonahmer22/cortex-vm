# Changelog

All notable changes to Cortex-VM are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [1.2.1] - 2026-05-12

### Fixed
- **`install.sh` PGO build was silently broken** -- `.gcda` profile files are stored in deeply nested subdirectories of `build/pgo/`; the previous `cp *.gcda` only matched the top level (nothing), so profile data was discarded before the PGO rebuild and every install produced a plain `-O3` binary without any profile guidance. Fixed with `cp -r`. A verification step now aborts with an error if no profile data is found rather than proceeding silently.
- **`restrict` aliases unused in sw/lw hot path** -- `cb`, `sb`, and `hp` were declared as `restrict`-qualified locals in `run()` but immediately voided; the sw/lw dispatch handlers were passing the non-restrict originals (`codeBase`, `stackBase`, `heap`) to `setWord`/`loadWord` instead, defeating the aliasing hint. Fixed so the restrict-qualified names are used throughout.
- **Benchmark fib_rec expected output wrong** -- `realworld_fib_rec.s` was updated to compute fib(35) (result: 9227465) but `benchmarks/conftest.py` and `benchmarks/run.py` still asserted fib(30) (result: 832040), causing the correctness check to silently report wrong results.

---

## [1.2.0] - 2026-05-12

### Added
- **Persistent VM context API** — `cortexVMCreate`, `cortexVMExecSource`, `cortexVMExecBinary`, `cortexVMDestroy` for REPL-style use from embedding projects; heap, stack, and register state persist across calls within a single context

### Changed
- **Binary renamed** from `cortex-vm` to `cortex`; `install.sh` / `uninstall.sh` updated accordingly

### Fixed
- **`CortexVM` made opaque in `cortex-vm.h`** — the public header no longer includes internal headers (`heap.h`, `arena.h`); previously, `#include "cortex-vm.h"` would fail to compile in any project that didn't have those internal headers on its include path, making `make lib` effectively unusable

---

## [1.1.2] - 2026-05-10

### Changed
- **~3x throughput improvement** across all instruction categories - cumulative result of the optimizations below

### Performance
- **Threaded dispatch in `run()`** - replaced the `switch`/`case` opcode loop with a pre-computed GCC computed-goto table keyed on `(opcode << 8 | funct)`; each instruction jumps directly to its handler label with no comparisons
- **Instruction decode cache** - `run()` now pre-decodes the entire code section into a `DecodedInstr` array before execution begins; fields (`handler`, `imm`, `rd`, `ra`, `rb`, `flags`) are read directly on the hot path without re-extracting them from the raw word each iteration
- **`run()` marked `__attribute__((hot))`** - hints to GCC that this is the hottest function in the binary, enabling more aggressive inlining and register allocation decisions
- **`step()` inlined into `run()`** - eliminated the function-call overhead on every instruction; the former `step()` body is now expanded directly inside the dispatch loop (~1.9x speedup on its own)
- **Register file accessed via `restrict` pointer** - `regs`, `codeBase`, and `stackBase` are aliased to `restrict`-qualified local pointers (`r`, `cb`, `sb`) so the compiler can keep values in registers across instructions without reload-store barriers
- **PC held in a local variable** - `uint64_t pc` shadows `r[PC]` for the duration of the dispatch loop; written back to the register file only on exits and syscalls, eliminating a memory round-trip per instruction

### Added
- **Cross-language benchmark suite** (`benchmarks/cross_lang/`) - compares Cortex-VM against Lua and WebAssembly (Wasmtime) on four workloads: iterative Fibonacci, recursive Fibonacci, Newton's method, and prime sieve
- **`benchmarks/compare.py`** - CLI runner for the cross-language suite; outputs a side-by-side timing table
- **PGO build in `install.sh`** - `install.sh` now runs a profile-guided optimization pass (instrument -> benchmark -> optimize) before installing the final binary; profile data is collected from the benchmark suite automatically

### Fixed
- **`-WPedantic` warning on computed-goto table** - `src/core.c` wraps the dispatch-table initializer in a `#pragma GCC diagnostic` block to suppress the "address of label" pedantic warning without disabling it globally
- **`fread` null-termination in `handle_source`** - `content` was null-terminated at `flen` (the requested file size) instead of `nread` (actual bytes read); a short read would leave uninitialized bytes before the null terminator
- **`system()` unused-result in `serverStart`** - replaced `(void)system(cmd)` with an explicit non-fatal check; browser-open failure is intentionally ignored

---

## [1.1.1] - 2026-04-27

### Fixed
- **Linux build warnings** - resolved all `-Wunused-result` warnings in `src/server.c`:
  - `pipe()` calls in `run_vm_argv`, `handle_debug_start`, and `handle_irun_start` now check the return value and return an error on failure
  - `fread()` in `handle_source` and `system()` in `serverStart` cast to `(void)` to signal intentional discard

---

## [1.1.0] - 2026-04-27

### Added
- **Visual IDE** (`-V` flag) - browser-based IDE served directly from the binary; no internet required
  - Source editor with GAS syntax highlighting and material-darker theme (CodeMirror 5, embedded at build time)
  - 64-register panel with live hex and decimal display; changed registers highlighted after each step
  - Combined stdin/stdout console - interactive programs receive input typed directly into the console
  - Bytecode tab - hex word view of the assembled binary with entry point and data offset markers
  - Memory tab - word-level view of code, stack, and heap regions (populated during debug sessions)
  - Docs tab - built-in opcode reference and syscall quick-reference
  - Debug mode - step, continue, breakpoints, and live register/memory inspection; stdin handled without blocking the server
  - Save button - writes editor contents back to the source file when opened with a path
  - Favicon embedded in binary (`cortex-logos/sq_blk.png`)
  - CodeMirror 5.65.16 bundled offline (MIT licensed; see `THIRD_PARTY_LICENSES`)
- **`-V` / `--visual` flag** - launches the IDE; optionally accepts a source file path (`-V hello.s`)
- **`-D` / `--dump-regs` flag** - prints all 64 registers as a JSON array to stderr after execution; used internally by the IDE and available for external tooling
- **`install.sh`** - builds and installs the binary, static library, and header to `/usr/local/{bin,lib,include}`; runs `git submodule update --init --recursive` and the pytest suite before installing; works on Linux and macOS
- **`uninstall.sh`** - removes all installed files and runs `ldconfig` on Linux
- **`THIRD_PARTY_LICENSES`** - attribution for bundled third-party components

### Changed
- **Version string** updated to `v1.1.0`

### Fixed
- **Partial socket writes** - `send_response` now uses a `write_all` loop; previously a single `write()` could send fewer bytes than requested, causing browsers to stall for 30 seconds waiting for the rest of a large response
- **`SIGPIPE` handling** - server no longer terminates when a browser closes a connection mid-response; `write()` returns `EPIPE` and the connection is closed cleanly
- **`EINTR` retry in `write_all`** - signal interruptions no longer produce truncated HTTP responses
- **Browser connection delay** - server now advertises `127.0.0.1` instead of `localhost`, avoiding a 30-second IPv6 connection timeout on systems where the firewall drops SYNs to `::1`
- **Dynamic request buffer** - `read_request` now doubles the buffer and `realloc`s when headers exceed 8 KB, fixing page load failures on Linux where browsers send larger request headers
- **`xdg-open` blocking** - backgrounded with `&` on Linux so the accept loop starts before the browser launches
- **Format specifiers** - all `%llu`/`%016llX` on `uint64_t` values in debug paths replaced with `FMT_U64`/`FMT_X64` macros; fixes warnings and incorrect output on Linux (LP64)
- **`unsigned char` embedded arrays** - `UI_HTML` and all vendor asset arrays now declared `unsigned char`; `_GNU_SOURCE` defined on Linux before any system headers
- **`realloc` memory leak** - all `buf = realloc(buf, n)` patterns replaced with a `tmp` pointer so the original allocation is not leaked on OOM
- **Register JSON buffer size** - `build_regs_json` buffer increased from `64*22` to `64*24` bytes; the old size could truncate the JSON for large register values
- **`{"source":null}` Content-Length** - was `14`, corrected to `15`; the missing byte caused a JSON parse error in the browser when no source file is loaded

---

## [1.0.0] - 2026-04-11

### Added
- **`bgeu` and `bleu` branch instructions** - unsigned greater-or-equal and unsigned less-or-equal, completing the full set of 10 branch comparisons (signed and unsigned variants of `<`, `<=`, `>`, `>=`, plus `==` and `!=`)
- **`SYS_HEAP_TOP`** (syscall `52`) - returns the address of the next word that would be allocated (`HEAP_ADDR + heapUsed`); equals `HEAP_ADDR` when the heap is empty. Allows guest-side allocators to inspect the current heap boundary. 3 new tests in `tests/test_heap.py`
- **File I/O tests** (`tests/test_file_io.py`) - 5 new pytest tests covering `SYS_FILE_READ` into heap and stack buffers, return value (word count), empty file, and a full write/read-back round-trip
- **Heap memory** - the heap address region (`0x0001000000000000`) is now fully implemented
  - `SYS_HEAP_GROW` (syscall `51`) - allocates N words on the heap and returns the base address; returns 0 on failure or if N=0. The heap is backed by a `realloc`-grown contiguous `uint64_t` array, making `setWord`/`loadWord` heap access O(1)
  - `heapDestroy()` - frees and resets heap state after each `run()` call; fixes a memory leak and a state-persistence bug when `cortexExecBinary` is called multiple times in the same process
  - `setWord` and `loadWord` now handle heap addresses with proper bounds checking
  - 11 pytest tests in `tests/test_heap.py` covering allocation, store/load, multi-word regions, overlap isolation, heap/stack independence, zero-word allocation, and large allocations
- **`CONTRIBUTING.md`** - developer guide covering environment setup, build targets, running tests and benchmarks, code style, and PR workflow
- **`SECURITY.md`** - security policy with private disclosure contact and scope description
- **`.github/ISSUE_TEMPLATE/bug_report.md`** - structured bug report template
- **`.github/ISSUE_TEMPLATE/feature_request.md`** - feature request template
- **`.github/PULL_REQUEST_TEMPLATE.md`** - PR description template
- **`.github/workflows/ci.yml`** - GitHub Actions CI workflow: checks out with submodules, installs dependencies, runs `make`, `make lib`, and `pytest` on every push and PR to `main`

### Changed
- **`SYS_FILE_READ` API** *(breaking)* - now takes `a1` as the destination buffer address (heap or stack) instead of silently writing to the stack at `sp`. Returns the number of words written (not counting the null terminator) in `a0` instead of echoing the buffer address back. Callers must allocate a buffer (e.g. via `SYS_HEAP_GROW`) and pass its address in `a1`
- **Version string** updated to `v1.0.0`

### Fixed
- **`SYS_FILE_READ` stack overflow** - bounds check used `bufAddr + wi >= STACKSIZE` which was always true (virtual address >> stack size), preventing any bytes from being written; corrected to `(bufAddr - STACK_ADDR + wi) >= (STACKSIZE / sizeof(uint64_t))`
- **`loadWord` out-of-bounds read in code region** - no upper bound was checked against `codeBaseSize`; reads past the end of the code arena now return 0 with an error message
- **`setWord`/`loadWord` out-of-bounds stack access** - no check against `STACKSIZE`; out-of-bounds stack reads return 0, writes are rejected with an error message
- **`SYS_READ_STR` integer underflow** - `maxLen - 1` with `maxLen = 0` wrapped to `UINT64_MAX`, causing unbounded writes; guarded with an early `break` when `maxLen == 0`
- **`SYS_RAND_R_INT` signed overflow UB** - `mx - mn` was evaluated as `int64_t` and could overflow for wide ranges; subtraction now performed in `uint64_t`
- **Disassembler `.data` string buffer overflow** - `sline[4096]` had no bounds check; the write guard is now `pos >= sizeof(sline) - 4` (leaving headroom for 2-char escape + closing `"\"\n"`)

### Added (continued)
- **Python benchmarking suite** (`benchmarks/`) - measures VM throughput across 7 categories: integer ALU (MIPS), M extension (MIPS), float ALU (MFLOPS), memory (MIPS), branches (BOPS), real-world programs (wall-clock ms), and assembler throughput (MB/s)
  - `benchmarks/run.py` - CLI entry point; `--category`, `--repeats`, `--no-graphs` flags
  - `benchmarks/bench_core.py` - `BenchmarkResult` dataclass and `BenchmarkRunner` (Python-side `perf_counter` timing; assembles once, runs N times, takes median)
  - `benchmarks/report.py` - console table and matplotlib graph generation (6 PNG files saved to `benchmarks/results/`)
  - `benchmarks/conftest.py` - 12 pytest performance-floor and correctness assertions (`pytest benchmarks/conftest.py -m benchmark`)
  - 20 assembly workloads in `benchmarks/asm/` covering all categories
  - Results saved as timestamped JSON in `benchmarks/results/`
- **`requirements.txt`** - lists Python dependencies (`pytest`, `matplotlib`) for the test suite and benchmarking tools

---

## [0.5.0] - Disassembler

### Added
- **Full disassembler** (`-d` flag) - converts any Cortex-VM binary back to assembly source
  - All base ISA instruction formats: R, I, S, L, B, SYS
  - M extension: MR, MI (multiply/divide)
  - F extension: FR, FI, FB (float arithmetic and float branches)
  - `.data` section reconstruction: detects null-terminated printable-ASCII sequences and emits quoted string literals; non-printable words fall back to decimal integers; escape sequences (`\n`, `\t`, `\r`, `\\`, `\"`) round-trip correctly
  - Round-trip fidelity: `assemble -> disassemble -> re-assemble` produces a functionally equivalent binary; output uses raw register names (`r0`–`r63`) and raw immediates
- **`LineList`** data structure (`include/list.h`, `src/list.c`) - dynamic list of heap-copied line strings with O(1) join allocation via `totalBytes` pre-accounting
- **`HEADER_LEN` constant** (`include/defs.h`) - replaces all hardcoded header-length literals across the assembler, VM core, library, header module, and disassembler
- **`cortexAssemble(source, outputPath)`** - new public API function; assembles source, writes binary to disk, and returns the binary as a heap-allocated buffer (`binary[1]` = word count); pairs with `cortexExecBinary` for compile-once-run-many workflows
- **42 disassembler round-trip pytest tests** - covers every instruction format, both extensions, and `.data` sections (strings, escape sequences, integers)
- **4 `cortexAssemble` pytest tests** - verifies the returned binary is runnable, exit codes propagate correctly, compile-once-run-twice, and output file is created on disk

### Changed
- **Binary header expanded from 4 to 5 words** - word 4 stores `dataOffset` (absolute word index of the first data word; 0 if no `.data` section). Binaries from v0.4.0 and earlier are not compatible.
- All header-length constants replaced with `HEADER_LEN` across all modules

### Fixed
- **`main` label was never found by the entry-point search** - `labelListFind` was called with `tmp + 5` (length 5, includes null terminator); stored labels have length 4 (`end` points to `:`). Entry point always fell back to `HEADER_LEN`, which was only accidentally correct when `main:` was the very first thing in the file
- **`SYS_TIME_SLEEP` slept 1000× too long on POSIX** - used `sleep(ms * 1000)` where `sleep()` takes seconds; now uses `usleep(ms * 1000)` (microseconds)
- **`line` counter not reset between `assemble()` calls** - error line numbers were wrong on the second and subsequent calls within the same process (e.g. via `cortexExecSource`)
- **`dataOffset` captured after data words were written** - the data section start was recorded inside `case OP_DATA:` after `getData()` had already filled the list, capturing `fileLength` instead of the true section start; now captured at the top of `.data` detection before any data words are appended

---

## [0.4.0] - Library API & CLI Improvements

### Added
- **Embedding API** - `cortexExecSource(const char *source)` and `cortexExecBinary(const uint64_t *binary, size_t wordCount)` in `src/cortex-vm.c` / `include/cortex-vm.h`; runs the VM with no CLI involvement
- **`make lib` target** - builds `lib/libcortex-vm.a` and copies `lib/cortex-vm.h`; everything an embedder needs in one directory
- **`LIBRARY.md`** - full guide covering submodule setup, Makefile integration, API reference, and examples

### Changed
- `-a` and `-d` flags no longer fall through to execution after assembling or disassembling. To assemble and run in one step, use `-ar` (or `-ra`). Running a pre-assembled binary directly (`./cortex <binary>`) is unchanged.
- `make clean` now also removes the `lib/` directory

### Fixed
- `const` correctness propagated throughout the assembler: `head`, `peek`, `start` cursor variables, `LabelNode.start`/`.end`, `labelListAppend`, `labelListFind`, and `cmpChars` all use `const char *`
- `headerValidate` no longer calls `free(buff)` before `exit()` on validation failure - redundant since the process is terminating, and removes a spurious `const`-discard warning
- `strtod`/`strtoll` calls in the assembler now use a local `char *endptr` intermediary to avoid discarding `const` from the source cursor

---

## [0.3.0] - F Extension & Test Suite

### Added
- **F extension** (`EXT_FLOAT`, bit 0) - full 64-bit IEEE 754 double-precision floating point
  - FR-type register instructions: `fadd`, `fsub`, `fmul`, `fdiv`
  - FI-type immediate/unary instructions: `faddi`, `fsubi`, `fmuli`, `fdivi`, `fsqrt`, `fabs`, `fneg`, `ftoi`, `ftoui`, `itof`, `uitof`
  - FB-type float branches: `fblt`, `fble`, `fbgt`, `fbge`
  - Float immediates encoded as 32-bit IEEE 754 singles; full double precision available via `.data` + `lw`
  - Float syscalls: `SYS_PRINT_FLOAT` (4), `SYS_READ_FLOAT` (14), `SYS_RAND_FLOAT` (24)
- **pytest test suite** - 146 tests covering every instruction, all syscalls, and both extensions
- Float literal support in `.data` section (stored as 64-bit doubles)
- Float literal support in immediate fields of `getImm()` (stored as 32-bit float bit patterns)
- Extension flags auto-set by the assembler based on detected opcodes

### Fixed
- `FN_ITOF`/`FN_UITOF` were converting `immBits` (always 0) instead of `regs[ra]`
- `OP_FB` was using `I_IMM` (32-bit) instead of `B_IMM` (36-bit S-type encoding)
- `OP_FB` branches were absolute (`regs[PC] = imm`) instead of PC-relative (`regs[PC] = (regs[PC]-1) + imm`)
- No-immediate list in the FI/MI/I case was keyed on funct code values that collide between F and M extensions, causing `divui`, `remi`, and `remui` to silently skip reading their immediate
- `getData()` missing `else` branch caused an extra zero word to be appended after every string literal in the `.data` section
- `fneg` immediate check was on `funct == 0x01` (always false) instead of `flags == 0x01`
- `SYS_PRINT_FLOAT` and `SYS_RAND_FLOAT` used `break` instead of `return true`, causing fall-through to the error handler

---

## [0.2.0] - M Extension

### Added
- **M extension** (`EXT_M`, bit 1) - integer multiply and divide
  - MR-type: `mul`, `mulh`, `mulhu`, `div`, `divu`, `rem`, `remu`
  - MI-type (immediate variants): `muli`, `divi`, `divui`, `remi`, `remui`
  - 128-bit intermediate products for `mulh`/`mulhu` via `__int128_t`/`__uint128_t`
- Extension flags field (word 3 of header) with `EXT_FLOAT` (bit 0) and `EXT_M` (bit 1)
- Assembler auto-detection and flag-setting for M and F extension opcodes
- GCC-15 detection in Makefile with fallback to system `gcc`

### Changed
- `step()` split into `handleSyscall()` and `handleExtensionOpcode()` to reduce instruction cache pressure
- Build switched from Apple Clang to GCC-15 via Homebrew for ~2× throughput improvement on macOS (`-O3 -march=native`)

### Fixed
- Project previously only built on macOS; cross-platform build restored

---

## [0.1.0] - Base ISA & Syscalls

### Added
- Complete base ISA: R, I, S, L, B, and SYS instruction types
- All base ALU operations: `add`/`addi`, `sub`/`subi`, `and`/`andi`, `or`/`ori`, `xor`/`xori`, `sll`/`slli`, `srl`/`srli`, `sra`/`srai`, `jmp`
- Memory instructions: `sw`, `lw`
- Branch instructions: `beq`, `bne`, `blt`, `bltu`
- System instructions: `halt`, `syscall`, `nop`, `break`
- Full base syscall set: print/read int/uint/char/float/string, rand (seed/int/r-int/float), file I/O (open/read/write/close), time (get/sleep)
- Assembler: full single-pass assembler with two-pass label resolution
  - Immediate formats: decimal, hex (`0x`), binary (`0b`), octal (`0o`), char literals
  - Label definitions and forward references
  - `.data` section: strings, integer literals
  - Comments via `;`
- Binary format v1: 4-word header (magic, length, entry point, extensions) - expanded to 5 words in v0.5.0
- 64-register file with aliases (`zero`, `pc`, `sp`, `ra`, `s0`–`s13`, `a0`–`a13`, `t0`–`t31`)
- Calling convention: argument registers (`a0`–`a12`), callee-saved (`s0`–`s13`), syscall number (`a13`)
- Arena-based memory management for code and stack regions
- Three-region address space: code (`0x0000...`), heap (`0x0001...`, reserved), stack (`0x0008...`)
- Stack pointer initialized to `0x0008000000000000` at startup

---

[1.2.1]: https://github.com/jonahmer22/cortex-vm/releases/tag/v1.2.1
[1.2.0]: https://github.com/jonahmer22/cortex-vm/releases/tag/v1.2.0
[1.1.2]: https://github.com/jonahmer22/cortex-vm/releases/tag/v1.1.2
[1.1.1]: https://github.com/jonahmer22/cortex-vm/releases/tag/v1.1.1
[1.1.0]: https://github.com/jonahmer22/cortex-vm/releases/tag/v1.1.0
[1.0.0]: https://github.com/jonahmer22/cortex-vm/releases/tag/v1.0.0
