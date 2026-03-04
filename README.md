# cortex-vm
A general purpose virtual instruction set architecture virtual machine intended for language runtime. Current ISA is subject to sweeping changes and modifications. 

Credit where credit it due this is highly inspired by my experience with RISCV, Overture, and LEG cpu designs/ISAs.

## Rules
- 64 bit words
- 2’s complement
- Words only, all offsets are word offsets not byte offsets, no byte shenanigans (I detest bytes)
- All free bits must be 0
- Pretend everything is big endian

## Registers
64 total registers
- [0:3] 64 bit specials (zero, pc, sp, ra)
- [4:63] 64 bit

## ALU
### R type (ALU stuff, only registers)
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | 6 bit rb | 4 bit flags | 26 free

1. add
2. sub
3. or
4. xor
5. and
6. sll
7. slr
8. sar

### I type (allows for 32 bit sign extended immediate)
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm [31:26] | 4 bit flags | imm [25:0]

1. addi
2. subi
3. ori
4. xori
5. andi
6. slli
7. slri
8. sari
9. jmp

## Memory
### S type (allows for 36 bit sign extended immediate)
8 bit opcode | 8 bit function | 6 bit ra | imm [35:30] | 6 bit rb | imm [29:0]

1. sw

### L type (allows for 36 bit sign extended immediate)
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm [35:0]

1. lw

## Branching
### B type (allows for 36 bit immediate sign extended)
8 bit opcode | 8 bit function | 6 bit ra | 6 bit rd | imm [35:0]

1. beq
2. bne
3. blt
4. bltu

## System
### 8 bit opcode | 8 bit function | 48 bits free

1. halt
2. syscall
3. nop
4. break