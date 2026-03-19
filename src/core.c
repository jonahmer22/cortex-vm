#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "../include/core.h"

#define OPCODE(w)		(((w) >> 56) & 0xff)
#define FUNCT(w)		(((w) >> 48) & 0xff)
#define FLAGS(w)		(((w) >> 26) & 0xf)
#define RA(w)			(((w) >> 42) & 0x3f)
#define RD(w)			(((w) >> 36) & 0x3f)
#define RB(w)			(((w) >> 30) & 0x3f)
// I-type: imm[31:26] at word bits 35:30, imm[25:0] at word bits 25:0
#define I_IMM(w)		(((((w) >> 30) & 0x3f) << 26) | ((w) & 0x3ffffff))
// S-type and B-type: imm[35:30] at word bits 41:36, imm[29:0] at word bits 29:0
#define S_IMM(w)		(((((w) >> 36) & 0x3f) << 30) | ((w) & 0x3fffffff))
#define L_IMM(w)		((w) & 0xfffffffff)
#define B_IMM(w)		S_IMM(w)
#define SYS_IMM(w)		((w) & 0xffffffffffff)
// sign extension
#define SIGN_EXT32(v)	((int64_t)(int32_t)(v))
#define SIGN_EXT36(v)	((int64_t)(((v) & (1ULL << 35)) ? ((v) | ~((1ULL << 36) - 1)) : (v)))

void run(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength){
	fileLength -= 4;

	bool running = true;
	while(running){
		// FETCH
		uint64_t instr = codeBase[regs[1]++];

		printf("PC %llu: 0x%016llX\n", regs[1] - 1, instr);

		// DECODE
			

		if(regs[1] >= fileLength)
			running = false;

		regs[0] = 0;
	}
}
