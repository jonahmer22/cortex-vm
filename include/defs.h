#ifndef DEFS_H
#define DEFS_H

#define OP_R 0x81
#define OP_I 0x82
#define OP_S 0x83
#define OP_L 0x84
#define OP_B 0x85
#define OP_SYS 0x86

#define FN_ADDSUB 0x01
#define FN_OR 0x02
#define FN_XOR 0x03
#define FN_AND 0x04
#define FN_SLL 0x05
#define FN_SR 0x06
#define FN_JMP 0x07

#define FN_SW 0x01
#define FN_LW 0x01

#define FN_BEQ 0x01
#define FN_BNE 0x02
#define FN_BLT 0x03
#define FN_BLTU 0x04

#define FN_HALT 0x01
#define FN_SYSCALL 0x02
#define FN_NOP 0x03
#define FN_BREAK 0x04

#define CODE_ADDR 0x0000000000000000
#define HEAP_ADDR 0x0001000000000000
#define STACK_ADDR 0x0008000000000000

#define STACKSIZE (1024*1024*sizeof(uint64_t))

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
