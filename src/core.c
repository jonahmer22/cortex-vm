#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "../include/core.h"
#include "../include/defs.h"

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
#define SIGN_EXT32(v)		((int64_t)(int32_t)(v))
#define SIGN_EXT36(v)		((int64_t)(((v) & (1ULL << 35)) ? ((v) | ~((1ULL << 36) - 1)) : (v)))

void run(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength){
	fileLength -= 4;

	bool running = true;
	while(running){
		// FETCH
		uint64_t instr = codeBase[regs[1]++];

		printf("PC %llu: 0x%016llX\n", regs[1] - 1, instr);

		// DECODE
		uint8_t opcode = OPCODE(instr);
		switch(opcode){
			case OP_R:{
				uint8_t funct 	= FUNCT(instr);
				uint8_t ra 	= RA(instr);
				uint8_t rd 	= RD(instr);
				uint8_t rb 	= RB(instr);
				uint8_t flags 	= FLAGS(instr);

				// switch based off of the function
				switch(funct){
					case FN_ADDSUB:{
						switch(flags){
							case 0x0:{
								break;
							}
							case 0x1:{
								break;
							}
							default:
								fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x0208, flags);
								exit(EXIT_FAILURE);
								break;
						}
						break;
					}
					case FN_OR:{
						break;
					}
					case FN_XOR:{
						break;
					}
					case FN_AND:{
						break;
					}
					case FN_SLL:{
						break;
					}
					case FN_SR:{
						switch(flags){
							case 0x0:{
								break;
							}
							case 0x1:{
								break;
							}
							default:
								fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x0209, flags);
								exit(EXIT_FAILURE);
								break;
						}
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			}
			case OP_I:{
				uint8_t funct	= FUNCT(instr);
				uint8_t ra	= RA(instr);
				uint8_t rd	= RD(instr);
				uint8_t flags	= FLAGS(instr);
				uint64_t imm	= SIGN_EXT32(I_IMM(instr));

				// switch based off of the function
				switch(funct){
					case FN_ADDSUB:{
						switch(flags){
							case 0x0:{
								break;
							}
							case 0x1:{
								break;
							}
							default:
								fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x020A, flags);
								exit(EXIT_FAILURE);
								break;
						}
						break;
					}
					case FN_OR:{
						break;
					}
					case FN_XOR:{
						break;
					}
					case FN_AND:{
						break;
					}
					case FN_SLL:{
						break;
					}
					case FN_SR:{
						switch(flags){
							case 0x0:{
								break;
							}
							case 0x1:{
								break;
							}
							default:
								fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x020B, flags);
								exit(EXIT_FAILURE);
								break;
						}
						break;
					}
					case FN_JMP:{
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0203, funct);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			}
			case OP_S:{
				uint8_t funct 	= FUNCT(instr);
				uint8_t ra	= RA(instr);
				uint8_t rb 	= RB(instr);
				uint64_t imm	= SIGN_EXT36(S_IMM(instr));

				switch(funct){
					case FN_SW:{
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0204, funct);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			}
			case OP_L:{
				uint8_t funct	= FUNCT(instr);
				uint8_t ra	= RA(instr);
				uint8_t rd	= RD(instr);
				uint64_t imm	= SIGN_EXT36(L_IMM(instr));

				switch(funct){
					case FN_LW:{
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0205, funct);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			}
			case OP_B:{
				uint8_t funct	= FUNCT(instr);
				uint8_t ra	= RA(instr);
				uint8_t rb	= RB(instr);
				uint64_t imm	= SIGN_EXT36(B_IMM(instr));

				switch(funct){
					case FN_BEQ:{
						break;
					}
					case FN_BNE:{
						break;
					}
					case FN_BLT:{
						break;
					}
					case FN_BLTU:{
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0206, funct);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			}
			case OP_SYS:{
				uint8_t funct	= FUNCT(instr);
				uint64_t imm	= SYS_IMM(instr);

				switch(funct){
					case FN_HALT:{
						break;
					}
					case FN_SYSCALL:{
						break;
					}
					case FN_NOP:{
						break;
					}
					case FN_BREAK:{
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0207, funct);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			}
			default:
				fprintf(stderr, "[FATAL 0x%04X]: Illegal opcode 0x%02X.\n", 0x0201, opcode);
				exit(EXIT_FAILURE);
				break;
		}

		// if SP is past the max size of the stack then error
		if(regs[2] >= ((1024*1024)+0x0008000000000000)){
			fprintf(stderr, "[FATAL 0x%04X]: Stack overflow.\n", 0x020F);
			exit(EXIT_FAILURE);
		}

		// if PC is past or at the fileLength then stop running
		if(regs[1] >= fileLength)
			running = false;

		// enforce r0 = 0
		regs[0] = 0;
	}
}
