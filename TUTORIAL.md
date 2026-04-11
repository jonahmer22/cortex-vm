# Cortex-VM Tutorial

This tutorial walks you from zero to writing real programs for Cortex-VM. No prior VM or assembly experience is required, though familiarity with any programming language helps.

---

## Table of Contents

1. [Building Cortex-VM](#1-building-cortex-vm)
2. [Hello, World](#2-hello-world)
3. [Registers and Arithmetic](#3-registers-and-arithmetic)
4. [Comparisons and Branches](#4-comparisons-and-branches)
5. [Loops](#5-loops)
6. [The Data Section](#6-the-data-section)
7. [Memory: Store and Load](#7-memory-store-and-load)
8. [Functions and the Stack](#8-functions-and-the-stack)
9. [The M Extension — Multiply and Divide](#9-the-m-extension--multiply-and-divide)
10. [The F Extension — Floating Point](#10-the-f-extension--floating-point)
11. [The Disassembler](#11-the-disassembler)
12. [Heap Memory](#12-heap-memory)
13. [Syscall Reference Quick Card](#13-syscall-reference-quick-card)

---

## 1. Building Cortex-VM

**Requirements:** GCC (GCC 15 recommended for best performance), Make, Python 3 + pytest (for tests).

```bash
git clone https://github.com/jonahmer22/cortex-vm
cd cortex-vm
git submodule init
git submodule update
make
```

This produces the `cortex-vm` binary in the project root.

**Assembling and running a program:**
```bash
./cortex-vm -a program.s           # assemble to a.out
./cortex-vm -a program.s -o out    # assemble to 'out'
./cortex-vm -ar program.s          # assemble to a.out and run immediately
./cortex-vm -ar program.s -o out   # assemble to 'out' and run immediately
./cortex-vm program.out            # run a pre-assembled binary
```

**Running the test suite:**
```bash
pytest tests/
```

---

## 2. Hello, World

Create `hello.s`:

```asm
main:
    addi a0, zero, msg      ; a0 = address of msg string
    addi a13, zero, 5       ; a13 = 5 (SYS_PRINT_STR)
    syscall
    addi a0, zero, 0        ; exit code 0
    addi a13, zero, 0       ; a13 = 0 (SYS_EXIT)
    syscall

.data
    msg: "Hello, World!\n"
```

Run it:
```bash
./cortex-vm -ar hello.s
Hello, World!
```

**What's happening:**

- `.data` declares a string, stored one character per word, null-terminated.
- `addi a0, zero, msg` loads the word address of `msg` into register `a0`.
- Syscall `5` (`SYS_PRINT_STR`) reads `a0` as a pointer and prints until it finds a null word.
- Syscall `0` (`SYS_EXIT`) terminates the program. The exit code is in `a0`.

---

## 3. Registers and Arithmetic

Cortex-VM has 64 registers, all 64-bit. The most important ones:

| Name | Purpose |
|------|---------|
| `zero` | Always reads 0. Writes are ignored. |
| `sp` | Stack pointer. |
| `ra` | Return address (for function calls). |
| `a0`–`a13` | Argument / return value registers. `a13` = syscall number. |
| `t0`–`t31` | Temporary registers. Use freely. |
| `s0`–`s13` | Saved registers. Preserve these across function calls. |

**Basic arithmetic:**

```asm
main:
    addi t0, zero, 10       ; t0 = 0 + 10 = 10
    addi t1, zero, 32       ; t1 = 32
    add  t2, t0, t1         ; t2 = t0 + t1 = 42

    ; print t2 as a signed decimal integer
    addi a0, t2, 0          ; a0 = t2 + 0 (copy t2 into a0)
    addi a1, zero, 0        ; a1 = 0 (format: decimal)
    addi a13, zero, 1       ; syscall 1 = SYS_PRINT_INT
    syscall                 ; prints "42"

    addi a0, zero, 0
    addi a13, zero, 0
    syscall
```

**All arithmetic instructions:**

```asm
add  t2, t0, t1     ; t2 = t0 + t1
addi t2, t0, 5      ; t2 = t0 + 5
sub  t2, t0, t1     ; t2 = t0 - t1
subi t2, t0, 3      ; t2 = t0 - 3
and  t2, t0, t1     ; t2 = t0 & t1
or   t2, t0, t1     ; t2 = t0 | t1
xor  t2, t0, t1     ; t2 = t0 ^ t1
sll  t2, t0, t1     ; t2 = t0 << t1  (shift left)
srl  t2, t0, t1     ; t2 = t0 >> t1  (logical right, fills with 0)
sra  t2, t0, t1     ; t2 = t0 >> t1  (arithmetic right, preserves sign)
```

Each has an immediate variant by appending `i` (`andi`, `ori`, `xori`, `slli`, `srli`, `srai`).

**`not` pseudo-instruction:**
```asm
xori t2, t0, -1    ; t2 = ~t0
```

**Immediate formats:**
```asm
addi t0, zero, 42       ; decimal
addi t0, zero, 0xFF     ; hex
addi t0, zero, 0b1010   ; binary
addi t0, zero, 'A'      ; character literal (= 65)
addi t0, zero, '\n'     ; escape sequence
```

---

## 4. Comparisons and Branches

Branches compare two registers and jump to a label if the condition holds. The jump target is PC-relative and is specified as a label.

```asm
beq  ra, rb, label   ; jump if ra == rb
bne  ra, rb, label   ; jump if ra != rb
blt  ra, rb, label   ; jump if ra < rb   (signed)
bltu ra, rb, label   ; jump if ra < rb   (unsigned)
bgt  ra, rb, label   ; jump if ra > rb   (signed)
bgtu ra, rb, label   ; jump if ra > rb   (unsigned)
bge  ra, rb, label   ; jump if ra >= rb  (signed)
ble  ra, rb, label   ; jump if ra <= rb  (signed)
```

**Example — print a message only if two values are equal:**

```asm
main:
    addi t0, zero, 5
    addi t1, zero, 5
    beq t0, t1, equal        ; branch if t0 == t1
    jmp t9, zero, done       ; else skip

equal:
    addi a0, zero, msg
    addi a13, zero, 5
    syscall

done:
    addi a0, zero, 0
    addi a13, zero, 0
    syscall

.data
    msg: "they are equal\n"
```

**Unconditional jump:**
```asm
jmp t9, zero, some_label    ; jump to label, save return address in t9 (ignored)
```

`jmp rd, ra, imm` sets `pc = ra + imm` and saves the next instruction address in `rd`. Passing `zero` as `rd` discards the return address; passing `zero` as `ra` makes the jump absolute.

---

## 5. Loops

A loop is just a branch that jumps backward.

**Count from 1 to 10 and print the final value:**

```asm
main:
    addi t0, zero, 0        ; counter = 0
    addi t1, zero, 10       ; limit = 10

loop:
    addi t0, t0, 1          ; counter++
    blt  t0, t1, loop       ; while counter < 10

    ; print result
    addi a0, t0, 0
    addi a1, zero, 0
    addi a13, zero, 1
    syscall                 ; prints "10"

    addi a0, zero, 0
    addi a13, zero, 0
    syscall
```

**Sum 1 + 2 + … + 100:**

```asm
main:
    addi t0, zero, 1        ; i = 1
    addi t1, zero, 101      ; limit = 101
    addi t2, zero, 0        ; sum = 0

sum_loop:
    add  t2, t2, t0         ; sum += i
    addi t0, t0, 1          ; i++
    blt  t0, t1, sum_loop   ; while i < 101

    addi a0, t2, 0
    addi a1, zero, 0
    addi a13, zero, 1
    syscall                 ; prints "5050"

    addi a0, zero, 0
    addi a13, zero, 0
    syscall
```

---

## 6. The Data Section

`.data` must appear after all code. It defines labeled values loaded at runtime.

```asm
.data
    my_string: "hello\n"          ; string: one word per character, null-terminated
    my_int:    42                  ; integer word
    my_neg:    -7
    my_hex:    0xDEADBEEF
    pi:        3.14159265358979    ; 64-bit double
```

Access strings with `SYS_PRINT_STR` and the label as an address:
```asm
addi a0, zero, my_string
addi a13, zero, 5
syscall
```

Load integer and float values from data with `lw`:
```asm
lw t0, zero, my_int     ; t0 = the word stored at my_int
lw t1, zero, pi         ; t1 = 64-bit double bits of pi
```

---

## 7. Memory: Store and Load

`sw` stores a value to an address; `lw` loads a value from an address.

```asm
sw ra, rb, imm      ; mem[ra + imm] = rb
lw rd, ra, imm      ; rd = mem[ra + imm]
```

All addresses are **word** addresses (not byte addresses). The stack lives at `0x0008000000000000` and `sp` is initialized there at startup.

**Store and retrieve a value on the stack:**

```asm
addi t0, zero, 99
sw   sp, t0, 0          ; store t0 at sp+0
lw   t1, sp, 0          ; load back into t1 (t1 = 99)
```

**Multiple values at sequential offsets:**
```asm
addi t0, zero, 10
addi t1, zero, 20
sw   sp, t0, 0          ; stack[sp+0] = 10
sw   sp, t1, 1          ; stack[sp+1] = 20
lw   t2, sp, 0          ; t2 = 10
lw   t3, sp, 1          ; t3 = 20
```

---

## 8. Functions and the Stack

**Call a function and return:**

```asm
jmp ra, zero, my_func   ; call: saves return address in ra, jumps to my_func
; execution resumes here after my_func returns

my_func:
    ; ... do work ...
    jmp zero, ra, 0     ; return: jumps to the address stored in ra
```

**Callee-saved register spill/restore:**

If a function uses `s0`–`s13`, it must save and restore them. The convention is to push them onto the stack in the prologue and pop in the epilogue.

```asm
my_func:
    ; --- prologue ---
    sw   sp, ra, 0          ; push return address
    addi sp, sp, 1
    sw   sp, s0, 0          ; push s0
    addi sp, sp, 1

    ; --- body ---
    add  s0, a0, a1         ; s0 = a0 + a1
    addi a0, s0, 0          ; return value in a0

    ; --- epilogue ---
    subi sp, sp, 1
    lw   s0, sp, 0          ; restore s0
    subi sp, sp, 1
    lw   ra, sp, 0          ; restore return address
    jmp  zero, ra, 0        ; return
```

**Full example — add_and_print(60, 9):**

```asm
main:
    addi a0, zero, 60
    addi a1, zero, 9
    addi s0, zero, 420      ; save a value in s0 to verify it survives the call

    jmp ra, zero, add_and_print

    ; after return: s0 should still be 420, a0 = 69
    addi a1, zero, 0
    addi a13, zero, 1
    syscall                 ; prints "69"

    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    addi a0, zero, 0
    addi a13, zero, 0
    syscall

add_and_print:
    sw   sp, ra, 0
    addi sp, sp, 1
    sw   sp, s0, 0
    addi sp, sp, 1

    add  s0, a0, a1
    addi a0, s0, 0

    subi sp, sp, 1
    lw   s0, sp, 0
    subi sp, sp, 1
    lw   ra, sp, 0
    jmp  zero, ra, 0
```

---

## 9. The M Extension — Multiply and Divide

Multiply and divide instructions are available when the binary is assembled with M extension opcodes — the assembler sets the `EXT_M` flag automatically.

```asm
; multiply
mul  t2, t0, t1     ; t2 = t0 * t1   (lower 64 bits, signed)
muli t2, t0, 5      ; t2 = t0 * 5

mulh  t2, t0, t1    ; t2 = upper 64 bits of t0 * t1 (signed)
mulhu t2, t0, t1    ; t2 = upper 64 bits of t0 * t1 (unsigned)

; divide (truncates toward zero)
div  t2, t0, t1     ; t2 = t0 / t1   (signed)
divi t2, t0, 4      ; t2 = t0 / 4
divu t2, t0, t1     ; t2 = t0 / t1   (unsigned)
divui t2, t0, 4     ; t2 = t0 / 4    (unsigned immediate)

; remainder
rem  t2, t0, t1     ; t2 = t0 % t1   (signed)
remi t2, t0, 6      ; t2 = t0 % 6
remu t2, t0, t1     ; t2 = t0 % t1   (unsigned)
remui t2, t0, 9     ; t2 = t0 % 9    (unsigned immediate)
```

**Example — compute 6 × 7 and print it:**

```asm
main:
    addi t0, zero, 6
    addi t1, zero, 7
    mul  t2, t0, t1

    addi a0, t2, 0
    addi a1, zero, 0
    addi a13, zero, 1
    syscall             ; prints "42"

    addi a0, zero, 0
    addi a13, zero, 0
    syscall
```

---

## 10. The F Extension — Floating Point

Float operations use the same 64 registers, reinterpreting their bits as IEEE 754 doubles. The assembler sets the `EXT_FLOAT` flag automatically when float instructions are used.

**Loading floats:**

Use `.data` for full double precision:
```asm
lw t0, zero, pi         ; loads 64-bit double from .data

.data
    pi: 3.14159265358979
```

Or use an immediate (limited to single-precision range):
```asm
faddi t0, zero, 3.14    ; t0 = 0.0 + 3.14 (float precision)
```

**Float arithmetic:**

```asm
fadd  t2, t0, t1        ; t2 = t0 + t1
fsub  t2, t0, t1        ; t2 = t0 - t1
fmul  t2, t0, t1        ; t2 = t0 * t1
fdiv  t2, t0, t1        ; t2 = t0 / t1
fsqrt t2, t0            ; t2 = sqrt(t0)
fabs  t2, t0            ; t2 = |t0|
fneg  t2, t0            ; t2 = -t0

; immediate variants
faddi t2, t0, 2.5       ; t2 = t0 + 2.5
fsubi t2, t0, 1.0
fmuli t2, t0, 0.5
fdivi t2, t0, 4.0
```

**Conversion:**
```asm
ftoi  t2, t0            ; t2 = (int64_t)t0    (truncates toward zero)
ftoui t2, t0            ; t2 = (uint64_t)t0
itof  t2, t0            ; t2 = (double)(int64_t)t0
uitof t2, t0            ; t2 = (double)(uint64_t)t0
```

**Float branches:**
```asm
fblt t0, t1, label      ; branch if t0 < t1
fble t0, t1, label      ; branch if t0 <= t1
fbgt t0, t1, label      ; branch if t0 > t1
fbge t0, t1, label      ; branch if t0 >= t1
```

**Printing floats:**

`SYS_PRINT_FLOAT` (syscall 4) reads `a0` as a float and `a1` as the number of decimal places.

```asm
addi a0, t2, 0          ; copy float register to a0
addi a1, zero, 6        ; 6 decimal places
addi a13, zero, 4
syscall
```

> **Note:** `SYS_PRINT_INT` and `SYS_PRINT_UINT` use `a1` as a **base/format** selector, not precision. Always set `a1 = 0` (decimal) before printing integers if you have previously printed a float.

**Full float example — sqrt(2):**

```asm
main:
    lw t0, zero, two
    fsqrt t2, t0

    addi a0, t2, 0
    addi a1, zero, 15       ; 15 decimal places
    addi a13, zero, 4
    syscall                 ; prints "1.414213562373095"

    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    addi a0, zero, 0
    addi a13, zero, 0
    syscall

.data
    two: 2.0
```

---

## 11. The Disassembler

The disassembler converts a compiled binary back into readable assembly source. This is useful for debugging, inspecting what the assembler produced, or understanding an existing binary.

```sh
./cortex-vm -d program.out             # disassemble to out.s
./cortex-vm -d program.out -o prog.s   # disassemble to a specific file
```

**What the output looks like:**

Given:
```asm
main:
    addi a0, zero, msg
    addi a13, zero, 5
    syscall
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
.data
    msg: "hello"
```

The disassembler produces something like:
```asm
main:
    addi r18, r0, 6
    addi r31, r0, 5
    syscall
    addi r18, r0, 0
    addi r31, r0, 0
    syscall
.data
    "hello"
```

A few things to note:

- **Register aliases become raw numbers** — `a0` → `r18`, `zero` → `r0`, etc. The rebuilt source uses the `r0`–`r63` names directly.
- **Label references become raw addresses** — `msg` is replaced with its word address (relative to the code base). The address is correct because the re-assembled binary has the same instruction count.
- **`.data` strings are reconstructed** — the disassembler detects null-terminated printable-ASCII sequences and emits them as quoted string literals, including escape sequences like `\n` and `\t`.
- **Round-trip fidelity** — `assemble → disassemble → re-assemble` produces a binary that behaves identically to the original.

**Disassemble and run immediately:**

```sh
./cortex-vm -dr program.out    # disassemble to out.s, then run out.s
```

---

## 12. Heap Memory

The heap region starts at `0x0001000000000000` and grows upward. Memory is allocated with the `SYS_HEAP_GROW` syscall (number `51`), which works like a primitive `sbrk`: it extends the heap by N words and returns the base address of the newly allocated region. The guest program is responsible for any free/GC layer on top.

### Allocating heap memory

```asm
addi a0, zero, 16       ; request 16 words
addi a13, zero, 51      ; SYS_HEAP_GROW
syscall                 ; a0 = base address of the 16-word region, or 0 on failure
```

After a successful call, `a0` holds a word address in the heap region. Passing `0` for N always returns `0` without allocating.

### Storing and loading through a heap pointer

Heap addresses work exactly like stack or code addresses — use `sw` and `lw`:

```asm
; assume a0 = base address from SYS_HEAP_GROW
addi t0, zero, 42
sw   a0, t0, 0      ; heap[base + 0] = 42
lw   t1, a0, 0      ; t1 = heap[base + 0]   (t1 = 42)
```

Use immediate offsets to access subsequent words:
```asm
sw   a0, t0, 0      ; heap[base + 0]
sw   a0, t1, 1      ; heap[base + 1]
sw   a0, t2, 2      ; heap[base + 2]
```

### Full example — allocate a 4-word buffer and fill it

```asm
main:
    addi a0, zero, 4        ; allocate 4 words
    addi a13, zero, 51
    syscall                 ; a0 = heap base

    addi s0, a0, 0          ; save base in s0

    addi t0, zero, 10
    addi t1, zero, 20
    addi t2, zero, 30
    addi t3, zero, 40
    sw   s0, t0, 0
    sw   s0, t1, 1
    sw   s0, t2, 2
    sw   s0, t3, 3

    lw   a0, s0, 2          ; load index 2 (= 30)
    addi a1, zero, 0
    addi a13, zero, 1
    syscall                 ; prints "30"

    addi a0, zero, 0
    addi a13, zero, 0
    syscall
```

### Notes

- Heap allocations persist for the lifetime of a single `run()` call. The heap is automatically freed when execution ends — no manual cleanup is required from guest code.
- Accessing an address outside the allocated region (i.e., past the last `SYS_HEAP_GROW` boundary) is a fatal error on writes and returns `0` on reads with an error message.
- The heap and stack are fully independent memory regions; writes to one cannot affect the other.

---

## 13. Syscall Reference Quick Card

Set `a13` to the call number, fill argument registers, then execute `syscall`. Return values appear in `a0`.

| `a13` | Name | Key Args | Returns |
|-------|------|----------|---------|
| 0 | EXIT | `a0`=code | — |
| 1 | PRINT_INT | `a0`=value, `a1`=fmt | — |
| 2 | PRINT_UINT | `a0`=value, `a1`=fmt | — |
| 3 | PRINT_CHAR | `a0`=char | — |
| 4 | PRINT_FLOAT | `a0`=float, `a1`=precision | — |
| 5 | PRINT_STR | `a0`=addr | — |
| 11 | READ_INT | `a1`=fmt | `a0` |
| 12 | READ_UINT | `a1`=fmt | `a0` |
| 13 | READ_CHAR | — | `a0` |
| 14 | READ_FLOAT | — | `a0` |
| 15 | READ_STR | `a0`=dest, `a1`=max | — |
| 21 | RAND_SEED | `a0`=seed | — |
| 22 | RAND_INT | — | `a0` |
| 23 | RAND_R_INT | `a0`=min, `a1`=max | `a0` |
| 24 | RAND_FLOAT | — | `a0` |
| 31 | FILE_OPEN | `a0`=path, `a1`=mode | `a0`=fd |
| 32 | FILE_READ | `a0`=fd, `a1`=buf addr | `a0`=words written |
| 33 | FILE_CLOSE | `a0`=fd | — |
| 34 | FILE_WRITE | `a0`=fd, `a1`=buf | — |
| 41 | TIME_GET | — | `a0`=ms |
| 42 | TIME_SLEEP | `a0`=ms | — |
| 51 | HEAP_GROW | `a0`=N words | `a0`=base addr |
| 52 | HEAP_TOP  | — | `a0`=top addr |

**Print format values (a1):** `0`=decimal, `1`=binary, `2`=octal, `3`=hex.

**File open modes (a1):** `0`=read, `1`=write, `2`=append.

---

*For the full ISA specification including binary format, encoding tables, and extension internals, see [SPEC.md](SPEC.md). For disassembler round-trip test examples, see [tests/test_disassembler.py](tests/test_disassembler.py). For heap test examples, see [tests/test_heap.py](tests/test_heap.py).*
