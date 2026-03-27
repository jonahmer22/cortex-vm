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

// memory functions
void setWord(uint64_t addr, uint64_t val, uint64_t* codeBase,/* uint64_t* heapBase,*/ uint64_t* stackBase){
	(void)codeBase;	// just to shut up the compiler

	if(addr < HEAP_ADDR){
		// just error out, idk why this would be useful
		fprintf(stderr, "[FATAL 0x%04X]: Illegal write to code region at 0x%016llX.\n", 0x0210, addr);
		exit(EXIT_FAILURE);
	}
	else if(addr < STACK_ADDR){
		// do nothing for right now
		// TODO: implement the heap
		fprintf(stderr, "[FATAL 0x%04X]: Heap not implemented.\n", 0x0211);
		exit(EXIT_FAILURE);
	}
	else{
		// memory must be on the stack
		stackBase[addr - STACK_ADDR] = val;
	}
}
uint64_t loadWord(uint64_t addr, uint64_t* codeBase,/* uint64_t* heapBase,*/ uint64_t* stackBase){
	(void)codeBase;	// just to shut up the compiler

	if(addr < HEAP_ADDR){
		// just error out, idk why this would be useful
		fprintf(stderr, "[FATAL 0x%04X]: Illegal read from code region at 0x%016llX.\n", 0x0212, addr);
		exit(EXIT_FAILURE);
	}
	else if(addr < STACK_ADDR){
		// do nothing for right now
		// TODO: implement the heap
		fprintf(stderr, "[FATAL 0x%04X]: Heap not implemented.\n", 0x0213);
		exit(EXIT_FAILURE);
	}
	else{
		// memory must be on the stack
		return stackBase[addr - STACK_ADDR];
	}
}

bool step(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength){
	bool running = true;

	// FETCH
	uint64_t instr = codeBase[regs[1]++];

	#ifdef DEBUG
	printf("[DEBUG]: PC %llu:\t\t0x%016llX\n", regs[1] - 1, instr);

	for(int i = 0; i < 64; i += 4){
		printf("reg[%d]\t= 0x%016llX\t", i, regs[i]);
		printf("reg[%d]\t= 0x%016llX\t", i + 1, regs[i + 1]);
		printf("reg[%d]\t= 0x%016llX\t", i + 2, regs[i + 2]);
		printf("reg[%d]\t= 0x%016llX", i + 3, regs[i + 3]);
		printf("\n");
	}

	#endif

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
							regs[rd] = (uint64_t)regs[ra] + (uint64_t)regs[rb];
							break;
						}
						case 0x1:{
							regs[rd] = (uint64_t)regs[ra] - (uint64_t)regs[rb];
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
					regs[rd] = (uint64_t)regs[ra] | (uint64_t)regs[rb];
					break;
				}
				case FN_XOR:{
					regs[rd] = (uint64_t)regs[ra] ^ (uint64_t)regs[rb];
					break;
				}
				case FN_AND:{
					regs[rd] = (uint64_t)regs[ra] & (uint64_t)regs[rb];
					break;
				}
				case FN_SLL:{
					regs[rd] = (uint64_t)regs[ra] << ((uint64_t)regs[rb] & 0x3F);
					break;
				}
				case FN_SR:{
					switch(flags){
						case 0x0:{
							regs[rd] = (uint64_t)regs[ra] >> ((uint64_t)regs[rb] & 0x3F);
							break;
						}
						case 0x1:{
							regs[rd] = (uint64_t)((int64_t)regs[ra] >> ((uint64_t)regs[rb] & 0x3F));
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
							regs[rd] = (uint64_t)regs[ra] + (uint64_t)imm;
							break;
						}
						case 0x1:{
							regs[rd] = (uint64_t)regs[ra] - (uint64_t)imm;
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
					regs[rd] = (uint64_t)regs[ra] | (uint64_t)imm;
					break;
				}
				case FN_XOR:{
					regs[rd] = (uint64_t)regs[ra] ^ (uint64_t)imm;
					break;
				}
				case FN_AND:{
					regs[rd] = (uint64_t)regs[ra] & (uint64_t)imm;
					break;
				}
				case FN_SLL:{
					regs[rd] = (uint64_t)regs[ra] << ((uint64_t)imm & 0x3F);
					break;
				}
				case FN_SR:{
					switch(flags){
						case 0x0:{
							regs[rd] = (uint64_t)regs[ra] >> ((uint64_t)imm & 0x3F);
							break;
						}
						case 0x1:{
							regs[rd] = (uint64_t)((int64_t)regs[ra] >> ((uint64_t)imm & 0x3F));
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
					uint64_t next = regs[1];
					regs[1] = (uint64_t)regs[ra] + (uint64_t)imm;
					regs[rd] = next;
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
					setWord((uint64_t)(regs[ra] + imm), (uint64_t)regs[rb], codeBase, stackBase);
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
					regs[rd] = loadWord((uint64_t)(regs[ra] + imm), codeBase, stackBase);
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
					if((uint64_t)regs[ra] == (uint64_t)regs[rb]){
						regs[1] = (regs[1] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BNE:{
					if((uint64_t)regs[ra] != (uint64_t)regs[rb]){
						regs[1] = (regs[1] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BLT:{
					if((int64_t)regs[ra] < (int64_t)regs[rb]){
						regs[1] = (regs[1] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BLTU:{
					if((uint64_t)regs[ra] < (uint64_t)regs[rb]){
						regs[1] = (regs[1] - 1) + (uint64_t)imm;
					}
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
					running = false;
					break;
				}
				case FN_SYSCALL:{
					// no syscalls are currently implemented out outlined
					break;
				}
				case FN_NOP:{
					break;	// funnily enough this is good enough already
				}
				case FN_BREAK:{
					fprintf(stderr, "[BREAK]: Breakpoint hit at PC %llu\n", regs[1] - 1);
					fprintf(stderr, "Registers:\n");

					for(int i = 0; i < 64; i++){
						fprintf(stderr, "  r%-2d: 0x%016llX\n", i, regs[i]);
					}

					fprintf(stderr, "Press enter to continue...\n");
					getchar();
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

	return running;
}

void run(uint64_t *regs, uint64_t *codeBase, uint64_t *stackBase, uint64_t fileLength){
	while(step(regs, codeBase, stackBase, fileLength));
}
