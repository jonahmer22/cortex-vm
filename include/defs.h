#ifndef DEFS_H
#define DEFS_H

#define VERSION     0x0000000000000001
#define HEADER_LEN  5   // number of 64-bit words in the binary header

// =============
// base opcodes
// =============

#define OP_R    0x81    // R-type:   rd = ra op rb
#define OP_I    0x82    // I-type:   rd = ra op imm
#define OP_S    0x83    // S-type:   mem[ra + imm] = rb
#define OP_L    0x84    // L-type:   rd = mem[ra + imm]
#define OP_B    0x85    // B-type:   if (ra op rb) pc += imm
#define OP_SYS  0x86    // SYS-type: system instruction

// assembler-only pseudo opcodes (never encoded in binary)
#define OP_LABEL    0x00
#define OP_DATA     0xFF

// R and I type function codes
#define FN_ADDSUB   0x01    // add (flags=0) / sub (flags=1)
#define FN_OR       0x02    // bitwise or
#define FN_XOR      0x03    // bitwise xor
#define FN_AND      0x04    // bitwise and
#define FN_SLL      0x05    // shift left logical
#define FN_SR       0x06    // shift right logical (flags=0) / arithmetic (flags=1)
#define FN_JMP      0x07    // rd = pc; pc = ra + imm
#define FN_SLT      0x08    // rd = ra < rb ? 1 : 0
#define FN_SLTU     0x09    // rd = ra < rb ? 1 : 0 (but unsigned this time)
#define FN_SEQ      0x0A    // rd = ra == rb ? 1 : 0

// S-type function codes
#define FN_SW       0x01    // store word: mem[ra + imm] = rb

// L-type function codes
#define FN_LW       0x01    // load word: rd = mem[ra + imm]

// B-type function codes
#define FN_BEQ      0x01    // branch if ra == rb
#define FN_BNE      0x02    // branch if ra != rb
#define FN_BLT      0x03    // branch if ra < rb  (signed)
#define FN_BLTU     0x04    // branch if ra < rb  (unsigned)
#define FN_BGE      0x05    // branch if ra >= rb
#define FN_BGT      0x06    // branch if ra > rb
#define FN_BGTU     0x07    // branch if ra > rb (unsigned)
#define FN_BLE      0x08    // branch if ra <= rb

// SYS-type function codes
#define FN_HALT     0x01    // stop execution
#define FN_SYSCALL  0x02    // system call; number in A13, args in A0-A12
#define FN_NOP      0x03    // no operation
#define FN_BREAK    0x04    // debugger breakpoint

// ================
// M extension (integer multiply/divide)
// ================

#define EXT_M   (1ULL << 1)

#define OP_MR   0xE1    // M R-type: rd = ra op rb
#define OP_MI   0xE2    // M I-type: rd = ra op imm

// M R and I type function codes
#define FN_MUL      0x01    // rd = ra * rb        (lower 64 bits)
#define FN_MULH     0x02    // rd = ra * rb        (upper 64 bits, signed)
#define FN_MULHU    0x03    // rd = ra * rb        (upper 64 bits, unsigned)
#define FN_DIV      0x04    // rd = ra / rb        (signed)
#define FN_DIVU     0x05    // rd = ra / rb        (unsigned)
#define FN_REM      0x06    // rd = ra % rb        (signed)
#define FN_REMU     0x07    // rd = ra % rb        (unsigned)

// ================
// F extension (64-bit IEEE 754 floats, reuses r0-r63)
// ================

#define EXT_FLOAT   (1ULL << 0)

#define OP_FR   0xF1    // float R-type: rd = ra op rb
#define OP_FI   0xF2    // float I-type: rd = ra op imm (imm bits reinterpreted as double)
#define OP_FB   0xF3    // float branch: if (ra op rb) pc += imm

// float R and I type function codes
#define FN_FADD     0x01    // rd = ra + rb
#define FN_FSUB     0x02    // rd = ra - rb  (flags=1: rd = -ra)
#define FN_FMUL     0x03    // rd = ra * rb
#define FN_FDIV     0x04    // rd = ra / rb
#define FN_FSQRT    0x05    // rd = sqrt(ra)
#define FN_FABS     0x06    // rd = |ra|
#define FN_FTOI     0x07    // rd = (int64_t)ra   (truncating)
#define FN_ITOF     0x08    // rd = (double)ra
#define FN_FTOUI    0x09    // rd = (uint64_t)ra  (truncating)
#define FN_UITOF    0x0A    // rd = (double)(uint64_t)ra

// float branch function codes
#define FN_FBLT     0x01    // branch if ra < rb
#define FN_FBLE     0x02    // branch if ra <= rb
#define FN_FBGT     0x03    // branch if ra > rb
#define FN_FBGE     0x04    // branch if ra >= rb

// ================
// memory map
// ================

#define CODE_ADDR   0x0000000000000000
#define HEAP_ADDR   0x0001000000000000
#define STACK_ADDR  0x0008000000000000

#define STACKSIZE (1024*1024*sizeof(uint64_t))

// ================
// syscalls
// ================

// args:
// A0 = exit code
#define SYS_EXIT        0

// args:
// A0 = int to print
// A1 = format: 0=decimal, 1=binary, 2=octal, 3=hex
#define SYS_PRINT_INT   1

// args:
// A0 = unsigned int to print
// A1 = format: 0=decimal, 1=binary, 2=octal, 3=hex
#define SYS_PRINT_UINT  2

// args:
// A0 = char to print
#define SYS_PRINT_CHAR  3

// args: (EXT_FLOAT required)
// A0 = float to print
// A1 = format: 0=decimal, 1=binary, 2=octal, 3=hex
#define SYS_PRINT_FLOAT 4

// args:
// A0 = address of null-terminated string to print
#define SYS_PRINT_STR   5

// args:
// A1 = format: 0=decimal, 1=binary, 2=octal, 3=hex
// rets:
// A0 = integer read
#define SYS_READ_INT    11

// args:
// A1 = format: 0=decimal, 1=binary, 2=octal, 3=hex
// rets:
// A0 = unsigned integer read
#define SYS_READ_UINT   12

// rets:
// A0 = char read
#define SYS_READ_CHAR   13

// rets: (EXT_FLOAT required)
// A0 = float read
// A1 = precision of float printed
#define SYS_READ_FLOAT  14

// args:
// A0 = address of string destination buffer
// A1 = max length to read
#define SYS_READ_STR    15

// args:
// A0 = integer seed
#define SYS_RAND_SEED   21

// rets:
// A0 = pseudo-random integer
#define SYS_RAND_INT    22

// args:
// A0 = min
// A1 = max
// rets:
// A0 = random integer in [min, max]
#define SYS_RAND_R_INT  23

// rets: (EXT_FLOAT required)
// A0 = random float in [0.0, 1.0]
#define SYS_RAND_FLOAT  24

// args:
// A0 = address of null-terminated path
// A1 = mode: 0=read, 1=write, 2=append
// rets:
// A0 = file descriptor
#define SYS_FILE_OPEN   31

// args:
// A0 = file descriptor
// A1 = address of destination buffer (heap or stack)
// rets:
// A0 = number of words written (not counting null terminator)
#define SYS_FILE_READ   32

// args:
// A0 = file descriptor
#define SYS_FILE_CLOSE  33

// args:
// A0 = file descriptor
// A1 = address of null-terminated buffer to write
#define SYS_FILE_WRITE  34

// rets:
// A0 = time in milliseconds since Jan 1, 1970
#define SYS_TIME_GET    41

// args:
// A0 = time to sleep in milliseconds
#define SYS_TIME_SLEEP  42

// args:
// A0 = words to allocate
// rets:
// A0 = address of allocated region, or 0 on failure
#define SYS_HEAP_GROW   51

// rets:
// A0 = address of next word that would be allocated (equals HEAP_ADDR when heap is empty)
#define SYS_HEAP_TOP    52

// ================
// registers
// ================

#define ZERO    0
#define PC      1
#define SP      2
#define RA      3
#define S0      4
#define S1      5
#define S2      6
#define S3      7
#define S4      8
#define S5      9
#define S6      10
#define S7      11
#define S8      12
#define S9      13
#define S10     14
#define S11     15
#define S12		16
#define S13		17
#define A0		18
#define A1		19
#define A2		20
#define A3		21
#define A4		22
#define A5		23
#define A6		24
#define A7		25
#define A8		26
#define A9		27
#define A10		28
#define A11		29
#define A12		30
#define A13		31
#define T0		32
#define T1		33
#define T2		34
#define T3		35
#define T4		36
#define T5		37
#define T6		38
#define T7		39
#define T8		40
#define T9		41
#define T10		42
#define T11		43
#define T12		44
#define T13		45
#define T14		46
#define T15		47
#define T16		48
#define T17		49
#define T18		50
#define T19		51
#define T20		52
#define T21		53
#define T22		54
#define T23		55
#define T24		56
#define T25		57
#define T26		58
#define T27		59
#define T28		60
#define T29		61
#define T30		62
#define T31		63

#endif

//  .:
