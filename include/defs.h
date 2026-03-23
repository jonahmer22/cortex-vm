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

#define FN_HALT 0x01
#define FN_SYSCALL 0x02
#define FN_NOP 0x03
#define FN_BREAK 0x04

#define STACKSIZE (1024*1024*sizeof(uint64_t))
