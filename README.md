# cortex-vm
A general purpose virtual instruction set architecture virtual machine intended for language runtime. Current ISA is subject to sweeping changes and modifications.

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
| r18-r31 | 14 | `r0-r13` | Caller-saved. Used for arguments and return values. |
| r32-r63 | 32 | `t0-t31` | General purpose temporaries. No convention. |

Register aliases (`s0`, `r0`, `t0`, etc.) are assembler-level names for their corresponding physical registers and are interchangeable with the raw register number.

---

## Calling Convention

### Argument Passing
Arguments are passed in `r0-r13` (physical `r18-r31`) starting from the low end. There is no stack-based argument passing defined at the ISA level - this is left to the compiler.

### Return Values
Return values are placed in `r0-r13` starting from the low end. The caller is expected to know how many values are returned and in which registers, as determined by the function signature. Up to 14 distinct values may be returned.

### Register Preservation
- `s0-s13` (physical `r4-r17`): Callee-saved. A function must preserve these across a call.
- `r0-r13` (physical `r18-r31`): Caller-saved. Not preserved across calls. The caller must save any needed values before issuing a call.
- `t0-t31` (physical `r32-r63`): Caller-saved temporaries. No preservation guarantee.

### Call and Return
Calls and returns are both performed with `jmp`:

```
jmp ra=<func>, rd=r3, imm=0   # call: jump to func, save return address in ra (r3)
jmp ra=r3, rd=zero, imm=0     # ret:  jump to return address, discard into zero
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
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm[35:0]
```
36 bit sign-extended immediate. Compares `ra` and `rd`. If the condition holds, sets `pc` to `pc + imm`. Branch targets are PC-relative and typically assembled from labels.

### System
```
8 bit opcode | 8 bit function | 48 bits free
```

---

## Instruction Set

### ALU - R Type
| Instruction | Operation |
|-------------|-----------|
| `add` | `rd = ra + rb` |
| `sub` | `rd = ra - rb` |
| `or` | `rd = ra \| rb` |
| `xor` | `rd = ra ^ rb` |
| `and` | `rd = ra & rb` |
| `sll` | `rd = ra << rb` |
| `slr` | `rd = ra >> rb` (logical) |
| `sar` | `rd = ra >> rb` (arithmetic) |

### ALU - I Type
| Instruction | Operation |
|-------------|-----------|
| `addi` | `rd = ra + imm` |
| `subi` | `rd = ra - imm` |
| `ori` | `rd = ra \| imm` |
| `xori` | `rd = ra ^ imm` |
| `andi` | `rd = ra & imm` |
| `slli` | `rd = ra << imm` |
| `slri` | `rd = ra >> imm` (logical) |
| `sari` | `rd = ra >> imm` (arithmetic) |
| `jmp` | `rd = pc + 1; pc = ra + imm` |

`jmp` is the universal jump/call/return instruction. `ra` holds the base address, `imm` is a signed offset applied to `ra`, and `rd` receives `pc + 1` (the next instruction, used as the return address). Passing `zero` as `rd` discards the return address.

### Memory
| Instruction | Format | Operation |
|-------------|--------|-----------|
| `sw` | S | `mem[ra + imm] = rb` |
| `lw` | L | `rd = mem[ra + imm]` |

All memory addresses and offsets are word offsets.

### Branching
| Instruction | Condition |
|-------------|-----------|
| `beq` | Branch if `ra == rd` |
| `bne` | Branch if `ra != rd` |
| `blt` | Branch if `ra < rd` (signed) |
| `bltu` | Branch if `ra < rd` (unsigned) |

Branch target is `pc + imm`, where `imm` is the sign-extended 36 bit immediate. `beq`/`bne` are bitwise comparisons and are sign-agnostic by nature.

### System
| Instruction | Description |
|-------------|-------------|
| `halt` | Stops execution. |
| `syscall` | Triggers a system call. Convention TBD. |
| `nop` | No operation. |
| `break` | Triggers a debug breakpoint. |

---

## Binary Format

Cortex-VM executables are a sequence of 64 bit words. The file begins with a fixed 4 word header followed immediately by instructions.

### Magic Number
The first word identifies the file as a Cortex-VM binary:

```
0x2E3A434F52540001
  .:    CORT    v1
```

`.:` is the human-readable signature, `CORT` identifies the format, and the final 16 bits are the format version number. The version must match what the VM expects or the file will be rejected.

### Header Layout

| Word | Field | Description |
|------|-------|-------------|
| 0 | Magic + Version | `0x2E3A434F52540001` |
| 1 | File length | Total file size in words, including the header. |
| 2 | Entry point | Word offset from the start of the file to the first instruction to execute. Minimum valid value is 4. |
| 3 | Extension flags | Bitfield of requested VM extensions. A VM that does not support a required extension must reject the file. |

Instructions begin at word 4.

### Extension Flags
Each bit in the extension flags word corresponds to a VM extension. Extensions are activated selectively per program - the VM only enables what the program requests.

No extensions are currently implemented. The following are speculative examples of what future extensions might look like:

| Bit | Extension | Description |
|-----|-----------|-------------|
| 0 | `g` | Stack-walking garbage collector. |
| 1 | `s` | String functionality. |
| 2-63 | - | Reserved. Must be 0. |

### Debug Marker
In debug builds, the end of the file may be terminated with the word `0x2E3A444541440001` (`.:DEAD` + version). The VM ignores this word in normal execution but may validate its presence in strict or debug mode.

---

## Memory Layout

The VM's address space is flat and word-indexed. It is divided into three regions separated by large fixed gaps to ensure they can never collide in practice. Each region is backed by an independent arena.

### Address Map

| Region | Base Address | Direction | Notes |
|--------|-------------|-----------|-------|
| Code | `0x0000000000000000` | - | Fixed size, loaded from binary. Never grows. |
| Heap | `0x0001000000000000` | Grows up | Not currently implemented. Reserved for future dynamic allocation. |
| Stack | `0x0008000000000000` | Grows up | `sp` initialized to base at startup. |

The gaps between regions are large enough that no feasible program can bridge them.

### Address Translation
When the VM resolves an address it determines the region by range check:

```
addr < 0x0001000000000000   → code arena
addr < 0x0008000000000000   → heap arena
otherwise                   → stack arena
```

### Region Details

**Code** - Allocated once at load time to exactly the program size (file length minus 4 header words). Read-only during execution.

**Heap** - Base address reserved. Not allocated until heap functionality is needed. TBD.

**Stack** - Allocated at init to a default size. `sp` is initialized to `0x0008000000000000`. Grows upward as the program pushes values.

---

## VM Initialization Sequence

The following steps must occur in order before the first instruction is fetched.

1. **Read file** - Load the binary into a raw word buffer.
2. **Validate header** - Check magic number and version. Reject if either does not match. Read file length and verify it matches the buffer size. Read entry point and verify it is >= 4. Read extension flags and verify all requested extensions are supported.
3. **Allocate code arena** - Size is `file_length - 4` words. Copy program words (skipping the header) into the arena.
4. **Allocate stack arena** - Default initial size. Initialize `sp` to `0x0008000000000000`.
5. **Initialize registers** - Zero all 64 registers. Set `pc` to `entry_point - 4`. Since the header is stripped and not copied into the code arena, `pc = 0` corresponds to the first instruction. The entry point from the header is file-relative, so subtracting 4 converts it to a code-relative offset.
6. **Activate extensions** - Enable any VM extensions specified in the extension flags.
7. **Begin fetch-decode-execute.**

---

## Notes

### Loading 64 Bit Constants
There is no dedicated load-upper-immediate instruction. Arbitrary 64 bit constants are materialized using a sequence of `addi`, `slli`, and `addi`:

```
addi r, zero, <upper bits>
slli r, r, <shift amount>
addi r, r, <lower bits>
```

The compiler is responsible for decomposing constants into this sequence.

### Syscall Convention
Not yet defined. TBD.