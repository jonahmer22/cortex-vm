# Cortex-VM — Implementation Specification

This document is the authoritative technical reference for the Cortex ISA, binary format, assembler, and VM implementation. It covers every detail needed to write a conforming assembler, emulator, or compiler backend targeting Cortex.

---

## Table of Contents

1. [Design Rules](#design-rules)
2. [Registers](#registers)
3. [Calling Convention](#calling-convention)
4. [Instruction Formats](#instruction-formats)
5. [Instruction Set](#instruction-set)
6. [Syscall Convention](#syscall-convention)
7. [Binary Format](#binary-format)
8. [Memory Layout](#memory-layout)
9. [VM Initialization Sequence](#vm-initialization-sequence)
10. [Extensions](#extensions)
    - [F Extension — 64-bit Floats](#f-extension--64-bit-floats-ext_float-bit-0)
    - [M Extension — Integer Multiply/Divide](#m-extension--integer-multiplydivide-ext_m-bit-1)
11. [Notes](#notes)

---

## Design Rules

- 64-bit words throughout — all values, addresses, and offsets are full words.
- 2's complement signed integers.
- Word-addressed: all offsets and addresses are **word** offsets, not byte offsets.
- All free (unused) bits in an instruction must be zero.
- Big-endian bit numbering within words.

---

## Registers

64 total registers, each 64 bits wide.

| Range | Count | Alias | Role |
|-------|-------|-------|------|
| r0 | 1 | `zero` | Hardwired zero. Writes are discarded; reads always return 0. |
| r1 | 1 | `pc` | Program counter. Points to the next instruction to fetch. |
| r2 | 1 | `sp` | Stack pointer. Initialized to `0x0008000000000000` at startup. |
| r3 | 1 | `ra` | Return address. Conventionally set by `jmp` calls. |
| r4–r17 | 14 | `s0`–`s13` | Callee-saved general purpose registers. |
| r18–r31 | 14 | `a0`–`a13` | Caller-saved. Used for arguments, return values, and syscall number (`a13`). |
| r32–r63 | 32 | `t0`–`t31` | Caller-saved temporaries. No preservation guarantee. |

Register aliases are case-insensitive in the assembler and interchangeable with raw `r<N>` notation. When the F extension is active, the same register file is reused for 64-bit IEEE 754 doubles — float instructions simply reinterpret the stored bits.

---

## Calling Convention

### Argument Passing

Arguments are passed in `a0`–`a12` (r18–r30) starting from the lowest register. `a13` is reserved as the syscall number register and must not be clobbered across a call. Stack-based argument passing is not defined at the ISA level.

### Return Values

Return values are placed in `a0`–`a12` starting from `a0`. The caller is responsible for knowing how many values are returned. Up to 13 distinct values may be returned in registers.

### Register Preservation

| Class | Registers | Preserved by |
|-------|-----------|-------------|
| Saved | `s0`–`s13` (r4–r17) | Callee |
| Argument/Return | `a0`–`a13` (r18–r31) | Caller |
| Temporary | `t0`–`t31` (r32–r63) | Caller |

### Call and Return

```asm
jmp ra, zero, target    ; call: jump to target, save return address in ra
jmp zero, ra, 0         ; ret:  jump to address stored in ra
```

`jmp rd, ra, imm` sets `rd = pc` (next instruction address) and `pc = ra + imm`. To discard the return address, pass `zero` as `rd`.

### Stack Convention

The stack grows upward from `0x0008000000000000`. `sp` points to the next free slot (post-increment push, pre-decrement pop).

```asm
; push reg
sw   sp, reg, 0
addi sp, sp, 1

; pop reg
subi sp, sp, 1
lw   reg, sp, 0
```

---

## Instruction Formats

All instructions are 64 bits wide.

### R Type — Register-to-Register ALU

```
[63:56] opcode (8) | [55:48] funct (8) | [47:42] ra (6) | [41:36] rd (6) | [35:30] rb (6) | [29:26] flags (4) | [25:0] reserved
```

Computes `ra op rb`, result written to `rd`.

### I Type — Immediate ALU / Jump

```
[63:56] opcode (8) | [55:48] funct (8) | [47:42] ra (6) | [41:36] rd (6) | [35:30] imm[31:26] (6) | [29:26] flags (4) | [25:0] imm[25:0]
```

32-bit sign-extended immediate. Computes `ra op imm`, result written to `rd`.

### S Type — Store

```
[63:56] opcode (8) | [55:48] funct (8) | [47:42] ra (6) | [41:36] imm[35:30] (6) | [35:30] rb (6) | [29:0] imm[29:0]
```

36-bit sign-extended immediate. Stores `rb` to word address `ra + imm`.

### L Type — Load

```
[63:56] opcode (8) | [55:48] funct (8) | [47:42] ra (6) | [41:36] rd (6) | [35:0] imm[35:0]
```

36-bit unsigned immediate. Loads from word address `ra + imm` into `rd`.

### B Type — Branch

```
[63:56] opcode (8) | [55:48] funct (8) | [47:42] ra (6) | [41:36] imm[35:30] (6) | [35:30] rb (6) | [29:0] imm[29:0]
```

36-bit sign-extended immediate. Compares `ra` and `rb`. If condition holds, sets `pc` to `pc + imm` (PC-relative).

### System

```
[63:56] opcode (8) | [55:48] funct (8) | [47:0] reserved
```

---

## Instruction Set

Instructions are identified by a two-level scheme: opcode selects the format and operation class; function code selects the specific operation within that class. Flags further modify behavior where two related operations share a function code.

### Opcode Map

| Opcode | Format | Description |
|--------|--------|-------------|
| `0x81` | R | Register-to-register ALU |
| `0x82` | I | Immediate ALU / jump |
| `0x83` | S | Store word |
| `0x84` | L | Load word |
| `0x85` | B | Branch |
| `0x86` | System | System instructions |

### ALU — R Type (`0x81`) and I Type (`0x82`)

R and I types share function codes and flag semantics. R operates on two registers; I operates on a register and a 32-bit sign-extended immediate.

| Function | Mnemonic (R / I) | Operation | Flag bit 0 |
|----------|-----------------|-----------|------------|
| `0x01` | `add` / `addi` | `rd = ra + rb/imm` | 0 = add, 1 = sub (`sub`/`subi`) |
| `0x02` | `or` / `ori` | `rd = ra \| rb/imm` | — |
| `0x03` | `xor` / `xori` | `rd = ra ^ rb/imm` | — |
| `0x04` | `and` / `andi` | `rd = ra & rb/imm` | — |
| `0x05` | `sll` / `slli` | `rd = ra << rb/imm` | — |
| `0x06` | `srl`/`sra` / `srli`/`srai` | `rd = ra >> rb/imm` | 0 = logical, 1 = arithmetic |
| `0x07` | `jmp` | `rd = pc; pc = ra + imm` | — (I type only) |

> **`not`** is not a dedicated instruction — use `xori rd, ra, -1`.

### Memory

| Function | Mnemonic | Opcode | Operation |
|----------|----------|--------|-----------|
| `0x01` | `sw` | `0x83` (S) | `mem[ra + imm] = rb` |
| `0x01` | `lw` | `0x84` (L) | `rd = mem[ra + imm]` |

All addresses and offsets are word-indexed.

### Branches — B Type (`0x85`)

| Function | Mnemonic | Condition |
|----------|----------|-----------|
| `0x01` | `beq` | `ra == rb` (bitwise, sign-agnostic) |
| `0x02` | `bne` | `ra != rb` |
| `0x03` | `blt` | `ra < rb` (signed) |
| `0x04` | `bltu` | `ra < rb` (unsigned) |
| `0x05` | `bge` | `ra >= rb` (signed) |
| `0x06` | `bgt` | `ra > rb` (signed) |
| `0x07` | `bgtu` | `ra > rb` (unsigned) |
| `0x08` | `ble` | `ra <= rb` (signed) |

Branch target is `pc + imm`.

### System (`0x86`)

| Function | Mnemonic | Description |
|----------|----------|-------------|
| `0x01` | `halt` | Stop execution immediately. |
| `0x02` | `syscall` | System call. Number in `a13`, args in `a0`–`a12`. |
| `0x03` | `nop` | No operation. |
| `0x04` | `break` | Debugger breakpoint. Dumps registers, waits for enter. |

---

## Syscall Convention

System calls are invoked with `syscall`. Call number in `a13`, arguments in `a0`–`a12`, return values written back into `a0`–`a12`.

### Base Syscalls

| Number | Name | Args | Returns | Description |
|--------|------|------|---------|-------------|
| `0` | `SYS_EXIT` | `a0`=exit code | — | Terminate. |
| `1` | `SYS_PRINT_INT` | `a0`=value, `a1`=format | — | Print signed integer. |
| `2` | `SYS_PRINT_UINT` | `a0`=value, `a1`=format | — | Print unsigned integer. |
| `3` | `SYS_PRINT_CHAR` | `a0`=char | — | Print a single character. |
| `4` | `SYS_PRINT_FLOAT` | `a0`=float, `a1`=precision | — | Print float with N decimal places. Requires F. |
| `5` | `SYS_PRINT_STR` | `a0`=address | — | Print null-terminated string. |
| `11` | `SYS_READ_INT` | `a1`=format | `a0`=value | Read signed integer from stdin. |
| `12` | `SYS_READ_UINT` | `a1`=format | `a0`=value | Read unsigned integer from stdin. |
| `13` | `SYS_READ_CHAR` | — | `a0`=char | Read one character from stdin. |
| `14` | `SYS_READ_FLOAT` | — | `a0`=float | Read float from stdin. Requires F. |
| `15` | `SYS_READ_STR` | `a0`=dest, `a1`=max len | — | Read string from stdin into buffer. |
| `21` | `SYS_RAND_SEED` | `a0`=seed | — | Seed the PRNG. |
| `22` | `SYS_RAND_INT` | — | `a0`=value | Pseudo-random integer. |
| `23` | `SYS_RAND_R_INT` | `a0`=min, `a1`=max | `a0`=value | Random integer in `[min, max]`. |
| `24` | `SYS_RAND_FLOAT` | — | `a0`=value | Random float in `[0.0, 1.0]`. Requires F. |
| `31` | `SYS_FILE_OPEN` | `a0`=path, `a1`=mode | `a0`=fd | Open file. mode: 0=read, 1=write, 2=append. |
| `32` | `SYS_FILE_READ` | `a0`=fd | `a0`=buffer addr | Read entire file into buffer. |
| `33` | `SYS_FILE_CLOSE` | `a0`=fd | — | Close file descriptor. |
| `34` | `SYS_FILE_WRITE` | `a0`=fd, `a1`=buffer addr | — | Write null-terminated buffer to file. |
| `41` | `SYS_TIME_GET` | — | `a0`=ms | Milliseconds since Unix epoch. |
| `42` | `SYS_TIME_SLEEP` | `a0`=ms | — | Sleep for N milliseconds. |
| `51` | `SYS_HEAP_GROW` | `a0`=N words | `a0`=base address | Allocate N words on the heap; returns the base word address of the new region (`0x0001…`). Returns 0 on failure or if N=0. The heap grows monotonically — the guest is responsible for any free/GC layer on top. |

**Format values (a1 for print/read int/uint):** `0`=decimal, `1`=binary, `2`=octal, `3`=hex.

---

## Binary Format

Cortex-VM executables are a flat sequence of 64-bit big-endian words. A fixed 5-word header precedes the instruction stream; the optional `.data` section is appended after the last instruction.

### Magic Number

```
0x2E3A434F52540001
  .:    CORT    v1
```

`.:` is the human-readable signature. `CORT` is the format identifier. The final 16 bits are the format version (`0x0001` = v1).

### Header Layout

| Word | Field | Description |
|------|-------|-------------|
| 0 | Magic + Version | `0x2E3A434F52540001` |
| 1 | File length | Total size in words (header + code + data). |
| 2 | Entry point | Word index of first instruction (minimum `HEADER_LEN`). Set from `main:` label. |
| 3 | Extension flags | Bitfield of required extensions. Auto-set by assembler. |
| 4 | Data offset | Absolute word index of the first data word; `0` if no `.data` section. |

### Extension Flags

| Bit | Constant | Extension |
|-----|----------|-----------|
| 0 | `EXT_FLOAT` | F — 64-bit IEEE 754 float operations |
| 1 | `EXT_M` | M — integer multiply and divide |
| 2–63 | — | Reserved. Must be 0. |

---

## Memory Layout

The address space is flat and word-indexed, partitioned into three arenas.

| Region | Base Address | Direction | Notes |
|--------|-------------|-----------|-------|
| Code | `0x0000000000000000` | Fixed | Loaded from binary. Readable as data via `lw`. |
| Heap | `0x0001000000000000` | Grows up | Allocated on demand via `SYS_HEAP_GROW`. Backed by a `realloc`-grown contiguous `uint64_t` array; `setWord`/`loadWord` access is O(1). |
| Stack | `0x0008000000000000` | Grows up | `sp` initialized to base at startup. |

Data section words are placed in the code arena immediately after the last instruction.

---

## VM Initialization Sequence

1. Read binary into word buffer.
2. Parse and validate header — check magic, version, file length, entry point, extension flags, and data offset.
3. Allocate code arena (`file_length - HEADER_LEN` words). Copy words from index `HEADER_LEN` onward (skip header).
4. Allocate stack arena. Set `sp` to `0x0008000000000000`.
5. Zero all 64 registers. Set `pc` to `entry_point - HEADER_LEN` (file-relative → code-relative).
6. Enable extensions from the extension flags word.
7. Begin fetch-decode-execute loop.
8. After `run()` returns (or on `SYS_EXIT`), call `heapDestroy()` to free heap state.

---

## Extensions

### F Extension — 64-bit Floats (`EXT_FLOAT`, bit 0)

Reuses the existing r0–r63 register file. Float instructions interpret register contents as IEEE 754 double-precision values. No separate register file is required.

Float immediates in FI-type instructions are encoded as 32-bit IEEE 754 single-precision values (widened to double at execution). Full double precision requires storing values in the `.data` section and loading them with `lw`.

#### Opcodes

| Opcode | Format | Description |
|--------|--------|-------------|
| `0xF1` | FR | Float register-to-register |
| `0xF2` | FI | Float immediate / unary |
| `0xF3` | FB | Float branch |

#### Float ALU Function Codes (FR / FI)

| Function | Mnemonic | Operation | Notes |
|----------|----------|-----------|-------|
| `0x01` | `fadd` / `faddi` | `rd = ra + rb/imm` | |
| `0x02` | `fsub` / `fsubi` / `fneg` | `rd = ra - rb/imm` | flags=1: `rd = -ra` (negation, no imm) |
| `0x03` | `fmul` / `fmuli` | `rd = ra * rb/imm` | |
| `0x04` | `fdiv` / `fdivi` | `rd = ra / rb/imm` | |
| `0x05` | `fsqrt` | `rd = sqrt(ra)` | FI, no immediate |
| `0x06` | `fabs` | `rd = \|ra\|` | FI, no immediate |
| `0x07` | `ftoi` | `rd = (int64_t)ra` | Truncating; FI, no immediate |
| `0x08` | `itof` | `rd = (double)(int64_t)ra` | FI, no immediate |
| `0x09` | `ftoui` | `rd = (uint64_t)ra` | Truncating; FI, no immediate |
| `0x0A` | `uitof` | `rd = (double)(uint64_t)ra` | FI, no immediate |

#### Float Branch Function Codes (FB)

| Function | Mnemonic | Condition |
|----------|----------|-----------|
| `0x01` | `fblt` | `ra < rb` |
| `0x02` | `fble` | `ra <= rb` |
| `0x03` | `fbgt` | `ra > rb` |
| `0x04` | `fbge` | `ra >= rb` |

`beq`/`bne` work on float registers since they compare bits (NaN edge cases aside). Do not use `blt`/`bltu` on floats — use `fblt` instead.

---

### M Extension — Integer Multiply/Divide (`EXT_M`, bit 1)

#### Opcodes

| Opcode | Format | Description |
|--------|--------|-------------|
| `0xE1` | MR | Multiply/divide register-to-register |
| `0xE2` | MI | Multiply/divide with immediate |

#### Function Codes (MR / MI)

| Function | Mnemonic | Operation | Notes |
|----------|----------|-----------|-------|
| `0x01` | `mul` / `muli` | `rd = (ra * rb/imm)[63:0]` | Lower 64 bits |
| `0x02` | `mulh` | `rd = (ra * rb)[127:64]` | Upper 64 bits, signed; MR only |
| `0x03` | `mulhu` | `rd = (ra * rb)[127:64]` | Upper 64 bits, unsigned; MR only |
| `0x04` | `div` / `divi` | `rd = ra / rb/imm` | Signed, truncates toward zero |
| `0x05` | `divu` / `divui` | `rd = ra / rb/imm` | Unsigned |
| `0x06` | `rem` / `remi` | `rd = ra % rb/imm` | Signed remainder |
| `0x07` | `remu` / `remui` | `rd = ra % rb/imm` | Unsigned remainder |

`mulh`/`mulhu` give the upper 64 bits of a full 128-bit product, useful for overflow detection and big integer arithmetic.

---

## Notes

### Assembler Format

**Instruction syntax:** `mnemonic dest, src1, src2/imm`

**Immediate formats:** decimal (`42`, `-7`), hex (`0xFF`), binary (`0b1010`), octal (`0o17`), char literal (`'A'`, `'\n'`), label reference.

**Labels:** defined with a colon suffix (`loop:`); used anywhere an immediate is expected.

**Comments:** `;` to end of line.

**Data section:** declared with `.data`, must follow all code.
```asm
.data
    msg:  "hello, world"
    nums: 1, 2, 0xFF
    pi:   3.14159265358979
```

Strings are stored one character per word, null-terminated. Float literals are stored as 64-bit IEEE 754 doubles.

### Loading 64-bit Constants

There is no load-upper-immediate instruction. Arbitrary 64-bit constants are materialized via shift-and-add sequences:
```asm
addi t0, zero, <upper bits>
slli t0, t0,   <shift amount>
addi t0, t0,   <lower bits>
```

### Performance

The interpreter runs at approximately 400–500 million instructions per second at `-O3` on modern hardware (tested with GCC on Apple M3 Pro), comparable to the Lua interpreter. This is the expected throughput for a general-purpose dispatch-loop VM.
