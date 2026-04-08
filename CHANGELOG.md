# Changelog

All notable changes to Cortex-VM are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Added
- **Python benchmarking suite** (`benchmarks/`) — measures VM throughput across 7 categories: integer ALU (MIPS), M extension (MIPS), float ALU (MFLOPS), memory (MIPS), branches (BOPS), real-world programs (wall-clock ms), and assembler throughput (MB/s)
  - `benchmarks/run.py` — CLI entry point; `--category`, `--repeats`, `--no-graphs` flags
  - `benchmarks/bench_core.py` — `BenchmarkResult` dataclass and `BenchmarkRunner` (Python-side `perf_counter` timing; assembles once, runs N times, takes median)
  - `benchmarks/report.py` — console table and matplotlib graph generation (6 PNG files saved to `benchmarks/results/`)
  - `benchmarks/conftest.py` — 12 pytest performance-floor and correctness assertions (`pytest benchmarks/conftest.py -m benchmark`)
  - 20 assembly workloads in `benchmarks/asm/` covering all categories
  - Results saved as timestamped JSON in `benchmarks/results/`
- **`requirements.txt`** — lists Python dependencies (`pytest`, `matplotlib`) for the test suite and benchmarking tools

---

## [0.5.0] — Disassembler

### Added
- **Full disassembler** (`-d` flag) — converts any Cortex-VM binary back to assembly source
  - All base ISA instruction formats: R, I, S, L, B, SYS
  - M extension: MR, MI (multiply/divide)
  - F extension: FR, FI, FB (float arithmetic and float branches)
  - `.data` section reconstruction: detects null-terminated printable-ASCII sequences and emits quoted string literals; non-printable words fall back to decimal integers; escape sequences (`\n`, `\t`, `\r`, `\\`, `\"`) round-trip correctly
  - Round-trip fidelity: `assemble → disassemble → re-assemble` produces a functionally equivalent binary; output uses raw register names (`r0`–`r63`) and raw immediates
- **`LineList`** data structure (`include/list.h`, `src/list.c`) — dynamic list of heap-copied line strings with O(1) join allocation via `totalBytes` pre-accounting
- **`HEADER_LEN` constant** (`include/defs.h`) — replaces all hardcoded header-length literals across the assembler, VM core, library, header module, and disassembler
- **`cortexAssemble(source, outputPath)`** — new public API function; assembles source, writes binary to disk, and returns the binary as a heap-allocated buffer (`binary[1]` = word count); pairs with `cortexExecBinary` for compile-once-run-many workflows
- **42 disassembler round-trip pytest tests** — covers every instruction format, both extensions, and `.data` sections (strings, escape sequences, integers)
- **4 `cortexAssemble` pytest tests** — verifies the returned binary is runnable, exit codes propagate correctly, compile-once-run-twice, and output file is created on disk

### Changed
- **Binary header expanded from 4 to 5 words** — word 4 stores `dataOffset` (absolute word index of the first data word; 0 if no `.data` section). Binaries from v0.4.0 and earlier are not compatible.
- All header-length constants replaced with `HEADER_LEN` across all modules

### Fixed
- **`main` label was never found by the entry-point search** — `labelListFind` was called with `tmp + 5` (length 5, includes null terminator); stored labels have length 4 (`end` points to `:`). Entry point always fell back to `HEADER_LEN`, which was only accidentally correct when `main:` was the very first thing in the file
- **`SYS_TIME_SLEEP` slept 1000× too long on POSIX** — used `sleep(ms * 1000)` where `sleep()` takes seconds; now uses `usleep(ms * 1000)` (microseconds)
- **`line` counter not reset between `assemble()` calls** — error line numbers were wrong on the second and subsequent calls within the same process (e.g. via `cortexExecSource`)
- **`dataOffset` captured after data words were written** — the data section start was recorded inside `case OP_DATA:` after `getData()` had already filled the list, capturing `fileLength` instead of the true section start; now captured at the top of `.data` detection before any data words are appended

---

## [0.4.0] — Library API & CLI Improvements

### Added
- **Embedding API** — `cortexExecSource(const char *source)` and `cortexExecBinary(const uint64_t *binary, size_t wordCount)` in `src/cortex-vm.c` / `include/cortex-vm.h`; runs the VM with no CLI involvement
- **`make lib` target** — builds `lib/libcortex-vm.a` and copies `lib/cortex-vm.h`; everything an embedder needs in one directory
- **`LIBRARY.md`** — full guide covering submodule setup, Makefile integration, API reference, and examples

### Changed
- `-a` and `-d` flags no longer fall through to execution after assembling or disassembling. To assemble and run in one step, use `-ar` (or `-ra`). Running a pre-assembled binary directly (`./cortex-vm <binary>`) is unchanged.
- `make clean` now also removes the `lib/` directory

### Fixed
- `const` correctness propagated throughout the assembler: `head`, `peek`, `start` cursor variables, `LabelNode.start`/`.end`, `labelListAppend`, `labelListFind`, and `cmpChars` all use `const char *`
- `headerValidate` no longer calls `free(buff)` before `exit()` on validation failure — redundant since the process is terminating, and removes a spurious `const`-discard warning
- `strtod`/`strtoll` calls in the assembler now use a local `char *endptr` intermediary to avoid discarding `const` from the source cursor

---

## [0.3.0] — F Extension & Test Suite

### Added
- **F extension** (`EXT_FLOAT`, bit 0) — full 64-bit IEEE 754 double-precision floating point
  - FR-type register instructions: `fadd`, `fsub`, `fmul`, `fdiv`
  - FI-type immediate/unary instructions: `faddi`, `fsubi`, `fmuli`, `fdivi`, `fsqrt`, `fabs`, `fneg`, `ftoi`, `ftoui`, `itof`, `uitof`
  - FB-type float branches: `fblt`, `fble`, `fbgt`, `fbge`
  - Float immediates encoded as 32-bit IEEE 754 singles; full double precision available via `.data` + `lw`
  - Float syscalls: `SYS_PRINT_FLOAT` (4), `SYS_READ_FLOAT` (14), `SYS_RAND_FLOAT` (24)
- **pytest test suite** — 146 tests covering every instruction, all syscalls, and both extensions
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

## [0.2.0] — M Extension

### Added
- **M extension** (`EXT_M`, bit 1) — integer multiply and divide
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

## [0.1.0] — Base ISA & Syscalls

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
- Binary format v1: 4-word header (magic, length, entry point, extensions) — expanded to 5 words in v0.5.0
- 64-register file with aliases (`zero`, `pc`, `sp`, `ra`, `s0`–`s13`, `a0`–`a13`, `t0`–`t31`)
- Calling convention: argument registers (`a0`–`a12`), callee-saved (`s0`–`s13`), syscall number (`a13`)
- Arena-based memory management for code and stack regions
- Three-region address space: code (`0x0000...`), heap (`0x0001...`, reserved), stack (`0x0008...`)
- Stack pointer initialized to `0x0008000000000000` at startup

---

[Unreleased]: https://github.com/jonahmer22/cortex-vm/compare/HEAD...HEAD
