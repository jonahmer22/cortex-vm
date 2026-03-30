#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// file descriptor table
#define MAX_FDS 64
static FILE *fdTable[MAX_FDS] = {NULL};

#include "../include/core.h"
#include "../include/defs.h"

#define OPCODE(w)		(((w) >> 56) & 0xff)
#define FUNCT(w)		(((w) >> 48) & 0xff)
#define FLAGS(w)		(((w) >> 26) & 0xf)
#define I_RA(w)			(((w) >> 42) & 0x3f)
#define I_RD(w)			(((w) >> 36) & 0x3f)
#define I_RB(w)			(((w) >> 30) & 0x3f)
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
		// allow reading of values from code section (usefull for .data)
		return codeBase[addr];
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

// returns false if the VM should stop running (SYS_EXIT)
static bool handleSyscall(uint64_t *regs, uint64_t *codeBase, uint64_t *stackBase, uint64_t *exit_code){
	switch(regs[A13]){
		// exit syscall
		case SYS_EXIT:{
			*exit_code = regs[A0];
			return false;
		}
		// printing syscalls
		case SYS_PRINT_INT:{
			switch(regs[A1]){
				case 0:{
					printf("%lld", (int64_t)regs[A0]);
					break;
				}
				case 1:{
					printf("0b");
					int64_t u = regs[A0];
					for(int i = 63; i >= 0; --i)
						putchar((u >> i) & 1 ? '1' : '0');
					break;
				}
				case 2:{
					printf("0o%022llo", (uint64_t)regs[A0]);
					break;
				}
				case 3:{
					printf("0x%016llX", (uint64_t)regs[A0]);
					break;
				}
				default:{
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall %llu.\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
				}
			}
			break;
		}
		case SYS_PRINT_UINT:{
			switch(regs[A1]){
				case 0:{
					printf("%llu", (uint64_t)regs[A0]);
					break;
				}
				case 1:{
					printf("0b");
					int64_t u = regs[A0];
					for(int i = 63; i >= 0; --i)
						putchar((u >> i) & 1 ? '1' : '0');
					break;
				}
				case 2:{
					printf("0o%022llo", (uint64_t)regs[A0]);
					break;
				}
				case 3:{
					printf("0x%016llX", (uint64_t)regs[A0]);
					break;
				}
				default:{
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall %llu.\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
				}
			}
			break;
		}
		case SYS_PRINT_CHAR:{
			printf("%c", (char)regs[A0]);
			break;
		}
		case SYS_PRINT_STR:{
			// print a string who's address is at A0
			for(size_t i = 0; ; i++){
				char chars = (char)loadWord(regs[A0] + i, codeBase, stackBase);
				if(chars == '\0'){
					break;
				}
				printf("%c", chars);
			}
			break;
		}
		// reading syscalls
		case SYS_READ_INT:{
			uint64_t v = 0;
			switch(regs[A1]){
				case 0:{
					scanf("%lld", (int64_t *)&v);
					break;
				}
				case 1:{	// TODO: binary not natively supported; read as octal fallback; idk what to do for this
					scanf("%llo", &v);
					break;
				}
				case 2:{
					scanf("%llo", &v);
					break;
				}
				case 3:{
					scanf("%llx", &v);
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall %llu.\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
			}
			regs[A0] = (uint64_t)v;
			// flush remaining input including newline
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF);
			break;
		}
		case SYS_READ_UINT:{
			uint64_t v = 0;
			switch(regs[A1]){
				case 0:{
					scanf("%llu", &v);
					break;
				}
				case 1:{
					scanf("%llo", &v);
					break;
				}
				case 2:{
					scanf("%llo", &v);
					break;
				}
				case 3:{
					scanf("%llx", &v);
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall %llu.\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
			}
			regs[A0] = v;
			// flush remaining input including newline
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF);
			break;
		}
		case SYS_READ_CHAR:{
			regs[A0] = (uint64_t)getchar();
			// flush remaining input including newline
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF);
			break;
		}
		case SYS_READ_STR:{
			uint64_t addr = regs[A0];
			uint64_t maxLen = regs[A1];
			size_t i = 0;
			int c;
			while(i < maxLen - 1 && (c = getchar()) != EOF && c != '\n'){
				setWord(addr + i, (uint64_t)(unsigned char)c, codeBase, stackBase);
				i++;
			}
			setWord(addr + i, 0, codeBase, stackBase);
			break;
		}
		// random number syscalls
		case SYS_RAND_SEED:{
			srand((unsigned int)regs[A0]);
			break;
		}
		case SYS_RAND_INT:{
			regs[A0] = ((uint64_t)rand() << 32) | (uint64_t)rand();
			break;
		}
		case SYS_RAND_R_INT:{
			int64_t mn = (int64_t)regs[A0];
			int64_t mx = (int64_t)regs[A1];
			if(mx < mn){
				fprintf(stderr, "[FATAL 0x%04X]: SYS_RAND_R_INT: max (%lld) is less than min (%lld).\n", 0x0213, mx, mn);
				exit(EXIT_FAILURE);
			}
			uint64_t range = (uint64_t)(mx - mn) + 1;
			regs[A0] = (uint64_t)(mn + (int64_t)(((uint64_t)rand() << 32 | (uint64_t)rand()) % range));
			break;
		}
		// file i/o syscalls
		case SYS_FILE_OPEN:{
			// build path string from vm memory
			char path[4096];
			size_t pi = 0;
			uint64_t addr = regs[A0];
			char c;
			while(pi < sizeof(path) - 1 && (c = (char)loadWord(addr++, codeBase, stackBase)) != '\0')
				path[pi++] = c;
			path[pi] = '\0';

			const char *mode;
			switch(regs[A1]){
				case 0:{
					mode = "rb";
					break;
				}
				case 1:{
					mode = "wb";
					break;
				}
				case 2:{
					mode = "ab";
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall %llu.\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
			}

			// find a free fd slot (skip 0,1,2 — stdin/stdout/stderr)
			int fd = -1;
			for(int fi = 3; fi < MAX_FDS; fi++){
				if(fdTable[fi] == NULL){ fd = fi; break; }
			}
			if(fd == -1){
				fprintf(stderr, "[FATAL 0x%04X]: File descriptor table full.\n", 0x020D);
				exit(EXIT_FAILURE);
			}

			fdTable[fd] = fopen(path, mode);
			if(!fdTable[fd]){
				fprintf(stderr, "[FATAL 0x%04X]: Could not open file \"%s\".\n", 0x0210, path);
				exit(EXIT_FAILURE);
			}
			regs[A0] = (uint64_t)fd;
			break;
		}
		case SYS_FILE_READ:{
			int fd = (int)regs[A0];
			if(fd < 0 || fd >= MAX_FDS || !fdTable[fd]){
				fprintf(stderr, "[FATAL 0x%04X]: Invalid file descriptor %d.\n", 0x0211, fd);
				exit(EXIT_FAILURE);
			}
			// read into stack at current sp
			uint64_t bufAddr = regs[SP];
			int c;
			uint64_t wi = 0;
			while((c = fgetc(fdTable[fd])) != EOF){
				stackBase[bufAddr - STACK_ADDR + wi] = (uint64_t)(unsigned char)c;
				wi++;
			}
			// null terminate
			stackBase[bufAddr - STACK_ADDR + wi] = 0;
			regs[A0] = bufAddr;
			break;
		}
		case SYS_FILE_CLOSE:{
			int fd = (int)regs[A0];
			if(fd >= 3 && fd < MAX_FDS && fdTable[fd]){
				fclose(fdTable[fd]);
				fdTable[fd] = NULL;
			}
			break;
		}
		case SYS_FILE_WRITE:{
			int fd = (int)regs[A0];
			if(fd < 0 || fd >= MAX_FDS || !fdTable[fd]){
				fprintf(stderr, "[FATAL 0x%04X]: Invalid file descriptor %d.\n", 0x0212, fd);
				exit(EXIT_FAILURE);
			}
			uint64_t addr = regs[A1];
			char c;
			while((c = (char)loadWord(addr++, codeBase, stackBase)) != '\0')
				fputc(c, fdTable[fd]);
			break;
		}
		// time syscalls
		case SYS_TIME_GET:{
			regs[A0] = (uint64_t)(time(NULL)) * 1000;
			break;
		}
		case SYS_TIME_SLEEP:{
			uint64_t ms = regs[A0];
			#ifdef _WIN32
			Sleep((DWORD)ms);
			#else
			sleep((uint64_t)(ms * 1000));
			#endif
			break;
		}
		case SYS_PRINT_FLOAT:{
			double v = 0;
			memcpy(&v, &regs[A0], sizeof(v));
			printf("%.*lf", (int)regs[A1], v);
			break;
		}
		case SYS_READ_FLOAT:{
			double v = 0;
			scanf("%lf", &v);
			memcpy(&regs[A0], &v, sizeof(v));
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF);
			break;
		}
		case SYS_RAND_FLOAT:{
			double v = ((double)rand() / (double)RAND_MAX);
			memcpy(&regs[A0], &v, sizeof(v));
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall %llu.\n", 0x020C, regs[A13]);
			exit(EXIT_FAILURE);
		}
	}
	return true;
}

// returns true if the opcode was handled by an extension, false if unknown
static bool handleExtensionOpcode(uint8_t opcode, uint64_t extensions, uint64_t instr, uint64_t *regs){
	bool handled = false;
	if(extensions & EXT_FLOAT){
		// we have float extensions and should check for them as opcodes
		switch(opcode){
			// no default so it falls through to other extension checks
			case OP_FR:{
				// decode values
				uint8_t funct = FUNCT(instr);
				uint8_t ra = I_RA(instr);
				uint8_t rd = I_RD(instr);
				uint8_t rb = I_RB(instr);

				// get values of ra and rb
				double raD, rbD;
				raD = rbD = 0;
				memcpy(&raD, &regs[ra], sizeof(raD));
				memcpy(&rbD, &regs[rb], sizeof(rbD));

				// switch on function
				switch(funct){
					case FN_FADD:{
						double res = raD + rbD;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FSUB:{
						double res = raD - rbD;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FMUL:{
						double res = raD * rbD;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FDIV:{
						double res = raD / rbD;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
						exit(EXIT_FAILURE);
						break;
				}
				handled = true;
				break;
			}
			case OP_FI:{
				// get values
				uint8_t funct = FUNCT(instr);
				uint8_t ra = I_RA(instr);
				uint8_t rd = I_RD(instr);
				uint8_t flags = FLAGS(instr);
				
				// propperly extract the data from the instruction
				uint32_t immBits = (uint32_t)I_IMM(instr);
				float immF = 0;
				memcpy(&immF, &immBits, sizeof(immF));
				double d = (double)immF;

				// get ra value
				double raD;
				raD = 0;
				memcpy(&raD, &regs[ra], sizeof(raD));
				
				// switch on function
				switch(funct){
					case FN_FADD:{
						double res = raD + d;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FSUB:{
						double res = 0;
						if(flags == 0x01){
							// neg instead of a sub
							res = -raD;
						}
						else{
							res = raD - d;
						}
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FMUL:{
						double res = raD * d;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FDIV:{
						double res = raD / d;
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FSQRT:{
						double res = sqrt(raD);
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FABS:{
						double res = fabs(raD);
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_FTOI:{
						regs[rd] = (int64_t)(raD);
						break;
					}
					case FN_FTOUI:{
						regs[rd] = (uint64_t)(raD);
						break;
					}
					case FN_ITOF:{
						double res = (double)(int64_t)regs[ra];
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					case FN_UITOF:{
						double res = (double)(uint64_t)regs[ra];
						memcpy(&regs[rd], &res, sizeof(res));
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
						exit(EXIT_FAILURE);
						break;
				}
				handled = true;
				break;
			}
			case OP_FB:{
				// decode values
				uint8_t funct = FUNCT(instr);
				uint8_t ra = I_RA(instr);
				uint8_t rb = I_RB(instr);
				int64_t imm = SIGN_EXT36(B_IMM(instr));

				// get ra value
				double raD, rbD;
				raD = rbD = 0;
				memcpy(&raD, &regs[ra], sizeof(raD));
				memcpy(&rbD, &regs[rb], sizeof(rbD));

				// switch on function
				switch(funct){
					case FN_FBLT:{
						if(raD < rbD){
							regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
						}
						break;
					}
					case FN_FBLE:{
						if(raD <= rbD){
							regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
						}
						break;
					}
					case FN_FBGT:{
						if(raD > rbD){
							regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
						}
						break;
					}
					case FN_FBGE:{
						if(raD >= rbD){
							regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
						}
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
						exit(EXIT_FAILURE);
						break;
				}
				handled = true;
				break;
			}
		}
	}
	if(!handled && extensions & EXT_M){
		// TODO: we have multiply extensions and should check for them as opcodes
		switch(opcode){
			// no default so it falls through to other extension checks
			case OP_MR:{
				// switch on function
				uint8_t funct = FUNCT(instr);
				uint8_t ra = I_RA(instr);
				uint8_t rd = I_RD(instr);
				uint8_t rb = I_RB(instr);
				// don't actually need flags I think
				// uint8_t flags = FLAGS(instr);
				switch(funct){
					case FN_MUL:{
						regs[rd] = (uint64_t)((int64_t)regs[ra] * (int64_t)regs[rb]);
						break;
					}
					case FN_MULH:{
						regs[rd] = (uint64_t)(((__int128_t)(int64_t)regs[ra] * (__int128_t)(int64_t)regs[rb]) >> 64);
						break;
					}
					case FN_MULHU:{
						regs[rd] = (uint64_t)(((__uint128_t)regs[ra] * (__uint128_t)regs[rb]) >> 64);
						break;
					}
					case FN_DIV:{
						regs[rd] = (uint64_t)((int64_t)regs[ra] / (int64_t)regs[rb]);
						break;
					}
					case FN_DIVU:{
						regs[rd] = (uint64_t)regs[ra] / (uint64_t)regs[rb];
						break;
					}
					case FN_REM:{
						regs[rd] = (uint64_t)((int64_t)regs[ra] % (int64_t)regs[rb]);
						break;
					}
					case FN_REMU:{
						regs[rd] = (uint64_t)regs[ra] % (uint64_t)regs[rb];
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
						exit(EXIT_FAILURE);
						break;
				}
				handled = true;
				break;
			}
			case OP_MI:{
				// switch on function
				uint8_t funct = FUNCT(instr);
				uint8_t ra = I_RA(instr);
				uint8_t rd = I_RD(instr);
				uint64_t imm = SIGN_EXT32(I_IMM(instr));
				// dont think this is actually needed
				// uint8_t flags = FLAGS(instr);
				switch(funct){
					case FN_MUL:{
						regs[rd] = (uint64_t)((int64_t)regs[ra] * (int64_t)imm);
						break;
					}
					// no need for high bits of OP_MI since I types can only have 32 bit imm values
					case FN_DIV:{
						regs[rd] = (uint64_t)((int64_t)regs[ra] / (int64_t)imm);
						break;
					}
					case FN_DIVU:{
						regs[rd] = (uint64_t)regs[ra] / (uint64_t)imm;
						break;
					}
					case FN_REM:{
						regs[rd] = (uint64_t)((int64_t)regs[ra] % (int64_t)imm);
						break;
					}
					case FN_REMU:{
						regs[rd] = (uint64_t)regs[ra] % (uint64_t)imm;
						break;
					}
					default:
						fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
						exit(EXIT_FAILURE);
						break;
				}
				handled = true;
				break;
			}
		}
	}
	return handled;
}

bool step(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code){
	bool running = true;

	// FETCH
	uint64_t instr = codeBase[regs[PC]++];

	#ifdef DEBUG
	printf("[DEBUG]: PC %llu:\t\t0x%016llX\n", regs[PC] - 1, instr);

	for(int i = 0; i < 64; i += 4){
		printf("reg[%d]\t= 0x%016llX\t", i, regs[i]);
		printf("reg[%d]\t= 0x%016llX\t", i + 1, regs[i + 1]);
		printf("reg[%d]\t= 0x%016llX\t", i + 2, regs[i + 2]);
		printf("reg[%d]\t= 0x%016llX", i + 3, regs[i + 3]);
		printf("\n");
	}
	#endif

	// DECODE + EXECUTE
	uint8_t opcode = OPCODE(instr);
	switch(opcode){
		case OP_R:{
			uint8_t funct 	= FUNCT(instr);
			uint8_t ra 	= I_RA(instr);
			uint8_t rd 	= I_RD(instr);
			uint8_t rb 	= I_RB(instr);
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
			uint8_t ra	= I_RA(instr);
			uint8_t rd	= I_RD(instr);
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
					uint64_t next = regs[PC];
					regs[PC] = (uint64_t)regs[ra] + (uint64_t)imm;
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
			uint8_t ra	= I_RA(instr);
			uint8_t rb 	= I_RB(instr);
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
			uint8_t ra	= I_RA(instr);
			uint8_t rd	= I_RD(instr);
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
			uint8_t ra	= I_RA(instr);
			uint8_t rb	= I_RB(instr);
			uint64_t imm	= SIGN_EXT36(B_IMM(instr));

			switch(funct){
				case FN_BEQ:{
					if((uint64_t)regs[ra] == (uint64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BNE:{
					if((uint64_t)regs[ra] != (uint64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BLT:{
					if((int64_t)regs[ra] < (int64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BLTU:{
					if((uint64_t)regs[ra] < (uint64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
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
			// uint64_t imm	= SYS_IMM(instr);
			switch(funct){
				case FN_HALT:{
					running = false;
					break;
				}
				case FN_SYSCALL:{
					if(!handleSyscall(regs, codeBase, stackBase, exit_code))
						running = false;
					break;
				}
				case FN_NOP:{
					break;	// funnily enough this is good enough already
				}
				case FN_BREAK:{
					fprintf(stderr, "[BREAK]: Breakpoint hit at PC %llu\n", regs[PC] - 1);
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
		default:{
			if(!handleExtensionOpcode(opcode, extensions, instr, regs))
				goto OP_FAILURE;
			break;

			OP_FAILURE:
			fprintf(stderr, "[FATAL 0x%04X]: Illegal opcode 0x%02X.\n", 0x0201, opcode);
			exit(EXIT_FAILURE);
		}
	}
	// if SP is past the max size of the stack then error
	if(regs[SP] >= ((1024*1024)+0x0008000000000000)){
		fprintf(stderr, "[FATAL 0x%04X]: Stack overflow.\n", 0x020F);
		exit(EXIT_FAILURE);
	}

	// if PC is past or at the fileLength then stop running
	if(regs[PC] >= fileLength)
		running = false;

	// enforce r0 = 0
	regs[ZERO] = 0;

	return running;
}

void run(uint64_t *regs, uint64_t *codeBase, uint64_t *stackBase, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code){
	while(step(regs, codeBase, stackBase, fileLength, extensions, exit_code));
}
