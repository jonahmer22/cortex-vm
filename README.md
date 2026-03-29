# cortex-vm
A general purpose virtual instruction set architecture virtual machine intended as the primary target architecture for a custom programming language. Current ISA is stable at v1 but subject to extension.

Credit where credit is due - this is highly inspired by experience with RISC-V, Overture, and LEG CPU designs/ISAs.

---

## Rules
- 64 bit words
- 2's complement
- Words only - all offsets are word offsets, not byte offsets. No byte shenanigans.
- All free bits must be 0
- Big endian

---

## Registers
64 total registers, all 64 bit.

| Range | Count | Alias | Role |
|-------|-------|-------|------|
| r0 | 1 | `zero` | Hardwired zero. Writes discarded, reads return 0. |
| r1 | 1 | `pc` | Program counter. |
| r2 | 1 | `sp` | Stack pointer. |
| r3 | 1 | `ra` | Return address. |
| r4-r17 | 14 | `s0-s13` | Callee-saved registers. |
| r18-r31 | 14 | `a0-a13` | Caller-saved. Used for arguments and return values. `a13` holds the syscall number. |
| r32-r63 | 32 | `t0-t31` | General purpose temporaries. No convention. |

Register aliases (`s0`, `a0`, `t0`, etc.) are assembler-level names for their corresponding physical registers and are interchangeable with the raw register number. When the F extension is active, all registers are reused for 64-bit IEEE 754 doubles — the bits are simply reinterpreted by float instructions.

---

## Calling Convention

### Argument Passing
Arguments are passed in `a0-a12` (physical `r18-r30`) starting from the low end. `a13` is reserved as the syscall number register and must not be clobbered across a call. There is no stack-based argument passing defined at the ISA level — this is left to the compiler.

### Return Values
Return values are placed in `a0-a12` starting from the low end. The caller is expected to know how many values are returned and in which registers, as determined by the function signature. Up to 13 distinct values may be returned.

### Register Preservation
- `s0-s13` (physical `r4-r17`): Callee-saved. A function must preserve these across a call.
- `a0-a13` (physical `r18-r31`): Caller-saved. Not preserved across calls. The caller must save any needed values before issuing a call.
- `t0-t31` (physical `r32-r63`): Caller-saved temporaries. No preservation guarantee.

### Call and Return
Calls and returns are both performed with `jmp`. `ra` holds the base address, `imm` is a signed offset, and `rd` receives `pc` as the return address, conventionally stored in `ra` (r3). Passing `zero` as `rd` discards the return address.

```asm
jmp ra, zero, target    ; call: jump to target, save return address in ra
jmp zero, ra, 0         ; ret:  jump to address in ra
```

### Stack Convention
The stack grows upward from `0x0008000000000000`. `sp` points to the next free slot.

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

### R Type - Register-to-Register ALU
```
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | 6 bit rb | 4 bit flags | 26 free
```
Performs an operation on `ra` and `rb`, result written to `rd`.

### I Type - Immediate ALU / Jump
```
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm[31:26] | 4 bit flags | imm[25:0]
```
32 bit sign-extended immediate. Performs an operation on `ra` and the immediate, result written to `rd`.

### S Type - Store
```
8 bit opcode | 8 bit function | 6 bit ra | imm[35:30] | 6 bit rb | imm[29:0]
```
36 bit sign-extended immediate. Stores `rb` to memory at address `ra + imm`.

### L Type - Load
```
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm[35:0]
```
36 bit sign-extended immediate. Loads from address `ra + imm` into `rd`.

### B Type - Branch
```
8 bit opcode | 8 bit function | 6 bit ra | imm[35:30] | 6 bit rb | imm[29:0]
```
36 bit sign-extended immediate. Compares `ra` and `rb`. If the condition holds, sets `pc` to `pc + imm`. Branch targets are PC-relative and assembled from labels.

### System
```
8 bit opcode | 8 bit function | 48 bits free
```

---

## Instruction Set

Instructions are identified by a two-level scheme: the opcode identifies the format and operation class, and the function code identifies the specific operation within that class. Flag bits further modify behavior within a function where hardware-equivalent operations share a function code.

### Opcode Map

| Opcode | Format | Description |
|--------|--------|-------------|
| `0x81` | R type | Register-to-register ALU |
| `0x82` | I type | Immediate ALU / jump |
| `0x83` | S type | Store |
| `0x84` | L type | Load |
| `0x85` | B type | Branch |
| `0x86` | System | System instructions |

### ALU - R Type (`0x81`) and I Type (`0x82`)

R type and I type share the same function codes and flag semantics. R type operates on two registers, I type operates on a register and a 32 bit sign-extended immediate.

| Function | Mnemonic (R / I) | Operation | Flag bit 0 |
|----------|-----------------|-----------|------------|
| `0x01` | `add` / `addi` | `rd = ra + rb/imm` | 0=add, 1=sub |
| `0x02` | `or` / `ori` | `rd = ra \| rb/imm` | - |
| `0x03` | `xor` / `xori` | `rd = ra ^ rb/imm` | - |
| `0x04` | `and` / `andi` | `rd = ra & rb/imm` | - |
| `0x05` | `sll` / `slli` | `rd = ra << rb/imm` | - |
| `0x06` | `srl`/`sra` / `srli`/`srai` | `rd = ra >> rb/imm` | 0=logical, 1=arithmetic |
| `0x07` | `jmp` | `rd = pc; pc = ra + imm` | - |

`not` is not a dedicated instruction — use `xori rd, ra, -1`. `jmp` is I type only.

### Memory

| Function | Mnemonic | Opcode | Operation |
|----------|----------|--------|-----------|
| `0x01` | `sw` | `0x83` (S) | `mem[ra + imm] = rb` |
| `0x01` | `lw` | `0x84` (L) | `rd = mem[ra + imm]` |

All memory addresses and offsets are word offsets.

### Branching - B Type (`0x85`)

| Function | Mnemonic | Condition |
|----------|----------|-----------|
| `0x01` | `beq` | Branch if `ra == rb` |
| `0x02` | `bne` | Branch if `ra != rb` |
| `0x03` | `blt` | Branch if `ra < rb` (signed) |
| `0x04` | `bltu` | Branch if `ra < rb` (unsigned) |

Branch target is `pc + imm`. `beq`/`bne` are bitwise comparisons and sign-agnostic.

### System (`0x86`)

| Function | Mnemonic | Description |
|----------|----------|-------------|
| `0x01` | `halt` | Stop execution. |
| `0x02` | `syscall` | System call. Number in `a13`, args in `a0-a12`. |
| `0x03` | `nop` | No operation. |
| `0x04` | `break` | Debug breakpoint. Dumps registers and waits for enter. |

---

## Syscall Convention

System calls are invoked with `syscall`. The call number is in `a13`, arguments in `a0`-`a12`, return values written back into `a0`-`a12`.

### Base Syscalls

| Number | Name | Args | Returns | Description |
|--------|------|------|---------|-------------|
| `0` | `SYS_EXIT` | `a0`=exit code | - | Terminate the program. |
| `1` | `SYS_PRINT_INT` | `a0`=value, `a1`=format | - | Print signed integer. |
| `2` | `SYS_PRINT_UINT` | `a0`=value, `a1`=format | - | Print unsigned integer. |
| `3` | `SYS_PRINT_CHAR` | `a0`=char | - | Print a single character. |
| `4` | `SYS_PRINT_FLOAT` | `a0`=float, `a1`=format | - | Print float. Requires F extension. |
| `5` | `SYS_PRINT_STR` | `a0`=address | - | Print null-terminated string. |
| `11` | `SYS_READ_INT` | `a1`=format | `a0`=value | Read signed integer from stdin. |
| `12` | `SYS_READ_UINT` | `a1`=format | `a0`=value | Read unsigned integer from stdin. |
| `13` | `SYS_READ_CHAR` | - | `a0`=char | Read a single character from stdin. |
| `14` | `SYS_READ_FLOAT` | - | `a0`=float | Read float from stdin. Requires F extension. |
| `15` | `SYS_READ_STR` | `a0`=dest addr, `a1`=max len | - | Read string from stdin into buffer. |
| `21` | `SYS_RAND_SEED` | `a0`=seed | - | Seed the PRNG. |
| `22` | `SYS_RAND_INT` | - | `a0`=value | Generate pseudo-random integer. |
| `23` | `SYS_RAND_R_INT` | `a0`=min, `a1`=max | `a0`=value | Generate random integer in `[min, max]`. |
| `24` | `SYS_RAND_FLOAT` | - | `a0`=value | Generate random float in `[0.0, 1.0]`. Requires F extension. |
| `31` | `SYS_FILE_OPEN` | `a0`=path addr, `a1`=mode | `a0`=fd | Open a file. mode: 0=read, 1=write, 2=append. |
| `32` | `SYS_FILE_READ` | `a0`=fd | `a0`=buffer addr | Read entire file into buffer. |
| `33` | `SYS_FILE_CLOSE` | `a0`=fd | - | Close a file descriptor. |
| `34` | `SYS_FILE_WRITE` | `a0`=fd, `a1`=buffer addr | - | Write null-terminated buffer to file. |
| `41` | `SYS_TIME_GET` | - | `a0`=ms | Milliseconds since Jan 1, 1970. |
| `42` | `SYS_TIME_SLEEP` | `a0`=ms | - | Sleep for given number of milliseconds. |

**Print/read format values:** `0`=decimal, `1`=binary, `2`=octal, `3`=hex.

---

## Binary Format

Cortex-VM executables are a sequence of 64 bit words. The file begins with a fixed 4 word header followed immediately by instructions, then optionally a `.data` section.

### Magic Number
```
0x2E3A434F52540001
  .:    CORT    v1
```
`.:` is the human-readable signature, `CORT` identifies the format, and the final 16 bits are the format version number.

### Header Layout

| Word | Field | Description |
|------|-------|-------------|
| 0 | Magic + Version | `0x2E3A434F52540001` |
| 1 | File length | Total file size in words, including header and data. |
| 2 | Entry point | Word offset from start of file to first instruction. Minimum valid value is 4. Set automatically by the assembler from the `main:` label. |
| 3 | Extension flags | Bitfield of required VM extensions. Set automatically by the assembler based on which opcodes were used. |

Instructions begin at word 4. Data (from `.data` section) is appended after all instructions.

### Extension Flags

Extension bits are set automatically by the assembler when it detects extension opcodes in use. The VM rejects a binary if it requests an extension that is not supported.

| Bit | Constant | Extension |
|-----|----------|-----------|
| 0 | `EXT_FLOAT` | F — 64-bit IEEE 754 float operations |
| 1 | `EXT_M` | M — integer multiply and divide |
| 2-63 | - | Reserved. Must be 0. |

---

## Assembler

The assembler converts `.s` source files into `.out` Cortex-VM binaries.

### Usage
```
cortex-vm -a <source.s>             # assemble to a.out
cortex-vm -a <source.s> -o <out>    # assemble to specified path
```

### Syntax

**Instructions** follow the format `mnemonic dest, src1, src2/imm`. Operands are separated by spaces or commas.

**Registers** are referenced by alias (`zero`, `sp`, `ra`, `s0`-`s13`, `a0`-`a13`, `t0`-`t31`) or raw number (`r0`-`r63`). Aliases are case-insensitive.

**Immediates** support the following formats:
- Decimal: `42`, `-7`
- Hex: `0xFF`
- Binary: `0b1010`
- Octal: `0o17`
- Char literal: `'A'`, `'\n'`, `'\t'`, `'\r'`, `'\0'`, `'\\'`, `'\''`
- Label reference: `loop`, `main` (resolved at assemble time)

**Labels** are defined with a colon suffix and can start with any letter, containing letters, digits, and underscores:
```asm
loop:
    addi t0, t0, 1
    blt t0, t1, loop
```

**Comments** begin with `;` and run to end of line.

**Data section** is declared with `.data` and must appear after all code. Each entry is a label followed by one or more comma-separated values. Strings are stored one character per word, null-terminated.
```asm
.data
    msg:  "hello, world"
    nums: 1, 2, 0xFF, 0b1010
    ch:   'A'
```

---

## Memory Layout

The VM's address space is flat and word-indexed, divided into three regions backed by independent arenas.

| Region | Base Address | Direction | Notes |
|--------|-------------|-----------|-------|
| Code | `0x0000000000000000` | - | Fixed size, loaded from binary. Readable as data. |
| Heap | `0x0001000000000000` | Grows up | Not yet implemented. |
| Stack | `0x0008000000000000` | Grows up | `sp` initialized to base at startup. |

Data section contents are loaded into the code arena after instructions and are addressed starting from `word 4 + instruction_count`.

---

## VM Initialization Sequence

1. Read binary into word buffer.
2. Parse and validate header — check magic, version, file length, entry point, and extension flags.
3. Allocate code arena (`file_length - 4` words). Copy program words (skipping header).
4. Allocate stack arena. Initialize `sp` to `0x0008000000000000`.
5. Zero all 64 registers. Set `pc` to `entry_point - 4` (converts file-relative offset to code-relative).
6. Activate extensions from extension flags word.
7. Begin fetch-decode-execute.

---

## Extensions

### F Extension — 64-bit Floats (`EXT_FLOAT`, bit 0)

Reuses the existing r0-r63 register file. Float instructions interpret register bits as IEEE 754 doubles. Requires no new register file.

#### Opcodes

| Opcode | Format | Description |
|--------|--------|-------------|
| `0xF1` | FR type | Float register-to-register |
| `0xF2` | FI type | Float immediate |
| `0xF3` | FB type | Float branch |

#### Float ALU Function Codes (FR / FI)

| Function | Mnemonic | Operation | Notes |
|----------|----------|-----------|-------|
| `0x01` | `fadd` | `rd = ra + rb` | |
| `0x02` | `fsub` / `fneg` | `rd = ra - rb` | flags=1: `rd = -ra` |
| `0x03` | `fmul` | `rd = ra * rb` | |
| `0x04` | `fdiv` | `rd = ra / rb` | |
| `0x05` | `fsqrt` | `rd = sqrt(ra)` | |
| `0x06` | `fabs` | `rd = \|ra\|` | |
| `0x07` | `ftoi` | `rd = (int64_t)ra` | truncating |
| `0x08` | `itof` | `rd = (double)ra` | |
| `0x09` | `ftoui` | `rd = (uint64_t)ra` | truncating |
| `0x0A` | `uitof` | `rd = (double)(uint64_t)ra` | |

#### Float Branch Function Codes (FB)

| Function | Mnemonic | Condition |
|----------|----------|-----------|
| `0x01` | `fblt` | Branch if `ra < rb` |
| `0x02` | `fble` | Branch if `ra <= rb` |
| `0x03` | `fbgt` | Branch if `ra > rb` |
| `0x04` | `fbge` | Branch if `ra >= rb` |

`beq`/`bne` work for floats since they are bitwise (NaN edge cases aside). `blt`/`bltu` do not — use `fblt` instead.

#### Float Syscalls
`SYS_PRINT_FLOAT` (4), `SYS_READ_FLOAT` (14), and `SYS_RAND_FLOAT` (24) require `EXT_FLOAT`.

---

### M Extension — Integer Multiply/Divide (`EXT_M`, bit 1)

#### Opcodes

| Opcode | Format | Description |
|--------|--------|-------------|
| `0xE1` | MR type | Multiply/divide register-to-register |
| `0xE2` | MI type | Multiply/divide immediate |

#### Function Codes (MR / MI)

| Function | Mnemonic | Operation | Notes |
|----------|----------|-----------|-------|
| `0x01` | `mul` | `rd = ra * rb` | lower 64 bits |
| `0x02` | `mulh` | `rd = ra * rb` | upper 64 bits, signed |
| `0x03` | `mulhu` | `rd = ra * rb` | upper 64 bits, unsigned |
| `0x04` | `div` | `rd = ra / rb` | signed |
| `0x05` | `divu` | `rd = ra / rb` | unsigned |
| `0x06` | `rem` | `rd = ra % rb` | signed |
| `0x07` | `remu` | `rd = ra % rb` | unsigned |

`mulh`/`mulhu` give the upper 64 bits of a 128-bit product, useful for overflow detection and big integer arithmetic.

---

## Notes

### Loading 64-bit Constants
There is no load-upper-immediate instruction. Arbitrary 64-bit constants are materialized using a sequence of shifts and adds:
```asm
addi t0, zero, <upper bits>
slli t0, t0,   <shift amount>
addi t0, t0,   <lower bits>
```
The compiler is responsible for decomposing constants into this sequence.

### Performance
The interpreter runs at approximately 400M instructions/second at `-O2` on modern hardware, comparable to the Lua interpreter. This is sufficient for a general-purpose language VM.
