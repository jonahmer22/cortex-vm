# cortex-vm
A general purpose virtual instruction set architecture virtual machine intended for language runtime. Current ISA is subject to sweeping changes and modifications.

Credit where credit is due — this is highly inspired by experience with RISC-V, Overture, and LEG CPU designs/ISAs.

---

## Rules
- 64 bit words
- 2's complement
- Words only — all offsets are word offsets, not byte offsets. No byte shenanigans.
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
| r4–r17 | 14 | `s0–s13` | Callee-saved registers. |
| r18–r31 | 14 | `r0–r13` | Caller-saved. Used for arguments and return values. |
| r32–r63 | 32 | `t0–t31` | General purpose temporaries. No convention. |

Register aliases (`s0`, `r0`, `t0`, etc.) are assembler-level names for their corresponding physical registers and are interchangeable with the raw register number.

---

## Calling Convention

### Argument Passing
Arguments are passed in `r0–r13` (physical `r18–r31`) starting from the low end. There is no stack-based argument passing defined at the ISA level — this is left to the compiler.

### Return Values
Return values are placed in `r0–r13` starting from the low end. The caller is expected to know how many values are returned and in which registers, as determined by the function signature. Up to 14 distinct values may be returned.

### Register Preservation
- `s0–s13` (physical `r4–r17`): Callee-saved. A function must preserve these across a call.
- `r0–r13` (physical `r18–r31`): Caller-saved. Not preserved across calls. The caller must save any needed values before issuing a call.
- `t0–t31` (physical `r32–r63`): Caller-saved temporaries. No preservation guarantee.

### Call and Return
Calls and returns are both performed with `jmp`:

```
jmp ra=<func>, rd=r3, imm=0   # call: jump to func, save return address in ra (r3)
jmp ra=r3, rd=zero, imm=0     # ret:  jump to return address, discard into zero
```

---

## Instruction Formats

All instructions are 64 bits wide.

### R Type — Register-to-Register ALU
```
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | 6 bit rb | 4 bit flags | 26 free
```
Performs an operation on `ra` and `rb`, result written to `rd`.

### I Type — Immediate ALU / Jump
```
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm[31:26] | 4 bit flags | imm[25:0]
```
32 bit sign-extended immediate. Performs an operation on `ra` and the immediate, result written to `rd`.

### S Type — Store
```
8 bit opcode | 8 bit function | 6 bit ra | imm[35:30] | 6 bit rb | imm[29:0]
```
36 bit sign-extended immediate. Stores `rb` to memory at address `ra + imm`.

### L Type — Load
```
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm[35:0]
```
36 bit sign-extended immediate. Loads from address `ra + imm` into `rd`.

### B Type — Branch
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

### ALU — R Type
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

### ALU — I Type
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

`.:`
