# Changelog

All notable changes to Cortex-VM are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

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
- Binary format v1: 4-word header (magic, length, entry point, extensions)
- 64-register file with aliases (`zero`, `pc`, `sp`, `ra`, `s0`–`s13`, `a0`–`a13`, `t0`–`t31`)
- Calling convention: argument registers (`a0`–`a12`), callee-saved (`s0`–`s13`), syscall number (`a13`)
- Arena-based memory management for code and stack regions
- Three-region address space: code (`0x0000...`), heap (`0x0001...`, reserved), stack (`0x0008...`)
- Stack pointer initialized to `0x0008000000000000` at startup

---

[Unreleased]: https://github.com/jonahmer22/cortex-vm/compare/HEAD...HEAD
