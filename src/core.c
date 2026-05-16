#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "../include/core.h"
#include "../include/defs.h"
#include "../include/cortex-vm.h"
#include "../include/vm_ctx.h"
#include "../include/heap.h"

// file descriptor table
#define MAX_FDS 64
static FILE *fdTable[MAX_FDS] ={NULL};

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

// pre-decoded instruction record produced once at run() entry.
// hot path reads only handler/imm/rd/ra/rb/flags; raw is for extension fallback.
typedef struct{
	void *handler;
	int64_t imm;
	uint8_t rd, ra, rb, flags;
	uint8_t opcode;
	uint8_t _pad[3];
	uint64_t raw;
} DecodedInstr;

// decoded instruction cache (see run()); promoted to file scope so it can be
// invalidated by runCacheReset() when a new binary is loaded into the same vm.
static DecodedInstr *decoded = NULL;
static uint64_t decoded_cap = 0;
static uint64_t *cached_cb = NULL;
static uint64_t cached_len = 0;

void runCacheReset(void){
	cached_cb = NULL;
	cached_len = 0;
}

// memory functions
void setWord(uint64_t addr, uint64_t val, uint64_t* codeBase, HeapState *heap, uint64_t *stackBase){
	(void)codeBase;	// just to shut up the compiler

	if(addr < HEAP_ADDR){
		// just error out, I don't know why this would be useful
		fprintf(stderr, "[FATAL 0x%04X]: Illegal write to code region at 0x" FMT_X64 ".\n", 0x0210, addr);
		exit(EXIT_FAILURE);
	}
	else if(addr < STACK_ADDR){
		if(heap->base == NULL || (addr - HEAP_ADDR) >= heap->used){
			fprintf(stderr, "[FATAL 0x%04X]: Heap write to unallocated address 0x" FMT_X64 ".\n", 0x0211, addr);
			exit(EXIT_FAILURE);
		}
		heap->base[addr - HEAP_ADDR] = val;
	}
	else{
		// memory must be on the stack
		if ((addr - STACK_ADDR) >= (STACKSIZE / sizeof(uint64_t))){
			fprintf(stderr, "[ERROR 0x%04X]: Write outside of stack bounds rejected.\n", 0xD041);
			// since we don't want to write just return early
			return;
		}
		stackBase[addr - STACK_ADDR] = val;
	}
}
uint64_t loadWord(uint64_t addr, uint64_t* codeBase, uint64_t codeBaseSize, HeapState *heap, uint64_t *stackBase){
	if(addr < HEAP_ADDR){
		if (addr >= codeBaseSize){
			fprintf(stderr, "[ERROR 0x%04X]: Code read address 0x" FMT_X64 " is out of bounds (arena size " FMT_U64 " words).\n", 0xD042, addr, codeBaseSize);
			// return early with an empty (0) value
			return 0;
		}
		// allow reading of values from code section (useful for .data)
		return codeBase[addr];
	}
	else if(addr < STACK_ADDR){
		if(heap->base == NULL || (addr - HEAP_ADDR) >= heap->used){
			fprintf(stderr, "[ERROR 0x%04X]: Heap read from unallocated address 0x" FMT_X64 ".\n", 0x0212, addr);
			return 0;
		}
		return heap->base[addr - HEAP_ADDR];
	}
	else{
		// memory must be on the stack
		if((addr - STACK_ADDR) >= (STACKSIZE / sizeof(uint64_t))){
			fprintf(stderr, "[ERROR 0x%04X]: Read outside of stack bounds rejected.\n", 0xD042);
			// since we don't want to write just return early with a 0
			return 0;
		}
		return stackBase[addr - STACK_ADDR];
	}
}

// returns a pointer to the heap data and the number of words currently allocated
uint64_t *heapSnapshot(HeapState *heap, uint64_t *out_used){
	*out_used = heap->used;
	return heap->base;
}

// returns false if the VM should stop running (SYS_EXIT)
static bool handleSyscall(CortexVM *vm, uint64_t *exit_code, uint64_t fileLength){
	uint64_t  *regs      = vm->regs;
	uint64_t  *codeBase  = vm->codeBase;
	HeapState *heap      = vm->heap;
	uint64_t  *stackBase = vm->stackBase;
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
					printf(FMT_I64, (int64_t)regs[A0]);
					break;
				}
				case 1:{
					printf("0b");
					int64_t u = (int64_t)regs[A0];
					for(int i = 63; i >= 0; --i)
						putchar((u >> i) & 1 ? '1' : '0');
					break;
				}
				case 2:{
					printf("0o" FMT_O64, (uint64_t)regs[A0]);
					break;
				}
				case 3:{
					printf("0x" FMT_X64, (uint64_t)regs[A0]);
					break;
				}
				default:{
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall " FMT_U64 ".\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
				}
			}
			break;
		}
		case SYS_PRINT_UINT:{
			switch(regs[A1]){
				case 0:{
					printf(FMT_U64, (uint64_t)regs[A0]);
					break;
				}
				case 1:{
					printf("0b");
					int64_t u = (int64_t)regs[A0];
					for(int i = 63; i >= 0; --i)
						putchar((u >> i) & 1 ? '1' : '0');
					break;
				}
				case 2:{
					printf("0o" FMT_O64, (uint64_t)regs[A0]);
					break;
				}
				case 3:{
					printf("0x" FMT_X64, (uint64_t)regs[A0]);
					break;
				}
				default:{
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall " FMT_U64 ".\n", 0x020C, regs[A13]);
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
			// print a string whose address is at A0
			for(size_t i = 0; ; i++){
				char chars = (char)loadWord(regs[A0] + i, codeBase, fileLength, heap, stackBase);
				if(chars == '\0'){
					break;
				}
				printf("%c", chars);
			}
			break;
		}
		// reading syscalls
		case SYS_READ_INT:{
			fflush(stdout);
			uint64_t v = 0;
			switch(regs[A1]){
				case 0:{
					if(scanf(SCN_I64, (int64_t *)&v) != 1){
						fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8011);
						v = 0;
					}
					break;
				}
				case 1:	// TODO: binary not natively supported; read as octal fallback; idk what to do for this
				case 2:{
					if (scanf(SCN_O64, &v) != 1){
						fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8012);
						v = 0;
					}
					break;
				}
				case 3:{
					if (scanf(SCN_x64, &v) != 1){
						fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8013);
						v = 0;
					}
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall " FMT_U64 ".\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
			}
			regs[A0] = (uint64_t)v;
			// flush remaining input including newline
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF) continue;
			break;
		}
		case SYS_READ_UINT:{
			fflush(stdout);
			uint64_t v = 0;
			switch(regs[A1]){
				case 0:{
					if (scanf(SCN_U64, &v) != 1){
						fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8014);
						v = 0;
					}
					break;
				}
				case 1:	// TODO: need to make a binary reading function in the future
				case 2:{
					if (scanf(SCN_O64, &v) != 1){
						fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8015);
						v = 0;
					}
					break;
				}
				case 3:{
					if (scanf(SCN_x64, &v) != 1){
						fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8016);
						v = 0;
					}
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall " FMT_U64 ".\n", 0x020C, regs[A13]);
					exit(EXIT_FAILURE);
			}
			regs[A0] = v;
			// flush remaining input including newline
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF) continue;
			break;
		}
		case SYS_READ_CHAR:{
			fflush(stdout);
			regs[A0] = (uint64_t)getchar();
			// flush remaining input including newline
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF) continue;
			break;
		}
		case SYS_READ_STR:{
			fflush(stdout);
			uint64_t addr = regs[A0];
			uint64_t maxLen = regs[A1];
			size_t i = 0;
			int c;
			if (maxLen == 0){
				fprintf(stderr, "[ERROR 0x%04X]: Underflow prevented on readstring syscall with maxlen 0.\n", 0xE026);
				break;
			}
			while(i < maxLen - 1 && (c = getchar()) != EOF && c != '\n'){
				setWord(addr + i, (uint64_t)(unsigned char)c, codeBase, heap, stackBase);
				i++;
			}
			setWord(addr + i, 0, codeBase, heap, stackBase);
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
				fprintf(stderr, "[FATAL 0x%04X]: SYS_RAND_R_INT: max (" FMT_I64 ") is less than min (" FMT_I64 ").\n", 0x0213, mx, mn);
				exit(EXIT_FAILURE);
			}
			uint64_t range = (uint64_t)mx - (uint64_t)mn + 1;
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
			while(pi < sizeof(path) - 1 && (c = (char)loadWord(addr++, codeBase, fileLength, heap, stackBase)) != '\0')
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
					fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall " FMT_U64 ".\n", 0x020C, regs[A13]);
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
			// read into caller-supplied buffer (a1 = destination address)
			uint64_t bufAddr = regs[A1];
			int c;
			uint64_t wi = 0;
			while((c = fgetc(fdTable[fd])) != EOF){
				setWord(bufAddr + wi, (uint64_t)(unsigned char)c, codeBase, heap, stackBase);
				wi++;
			}
			// null terminate
			setWord(bufAddr + wi, 0, codeBase, heap, stackBase);
			regs[A0] = wi;	// return number of words written (not counting null terminator)
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
			while((c = (char)loadWord(addr++, codeBase, fileLength, heap, stackBase)) != '\0')
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
			sleep((unsigned int)(ms / 1000));
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
			fflush(stdout);
			double v = 0;
			if (scanf("%lf", &v) != 1){
				fprintf(stderr, "[ERROR 0x%04X]: Improper input.\n", 0x8017);
				v = 0;
			}
			memcpy(&regs[A0], &v, sizeof(v));
			int ch;
			while((ch = getchar()) != '\n' && ch != EOF) continue;
			break;
		}
		case SYS_RAND_FLOAT:{
			double v = ((double)rand() / (double)RAND_MAX);
			memcpy(&regs[A0], &v, sizeof(v));
			break;
		}
		case SYS_HEAP_TOP:{
			if(heap == NULL)
				heap = heapStateCreate();

			regs[A0] = HEAP_ADDR + heap->used;
			break;
		}
		case SYS_HEAP_GROW:{
			if(heap == NULL)
				heap = heapStateCreate();

			uint64_t nwords = regs[A0];
			if(nwords == 0){
				regs[A0] = 0;
				break;
			}

			if(heap->used + nwords > heap->cap){
				uint64_t newCap = heap->used + nwords;
				uint64_t *newBase = realloc(heap->base, newCap * sizeof(uint64_t));
				if (newBase == NULL){
					regs[A0] = 0;
					break;
				}
				heap->base = newBase;
				heap->cap = newCap;
			}

			regs[A0] = heap->used + HEAP_ADDR;
			heap->used += nwords;

			#ifdef DEBUG
			printf("[DEBUG]: Heap expanded to " FMT_U64 ", " FMT_U64 " words allocated.\n", heapCap, nwords);
			#endif

			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent syscall " FMT_U64 ".\n", 0x020C, regs[A13]);
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
				
				// properly extract the data from the instruction
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
				}
				handled = true;
				break;
			}
			default:{
				handled = false;
				break;
			}
		}
	}
	if(!handled && extensions & EXT_M){
		// we have multiple extensions and should check for them as opcodes
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
				// don't think this is actually needed
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
				}
				handled = true;
				break;
			}
			default:{
				handled = false;
				break;
			}
		}
	}
	return handled;
}

// im dumb asf this made such a massive difference I should've been doing inline for this from the start
inline bool step(CortexVM *vm, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code){
	bool running = true;
	uint64_t *regs      = vm->regs;
	uint64_t *codeBase  = vm->codeBase;
	uint64_t *stackBase = vm->stackBase;
	HeapState *heap     = vm->heap;

	// FETCH
	uint64_t instr = codeBase[regs[PC]++];

	#ifdef DEBUG
	printf("[DEBUG]: PC " FMT_U64 ":\t\t0x" FMT_X64 "\n", regs[PC] - 1, instr);

	for(int i = 0; i < 64; i += 4){
		printf("reg[%d]\t= 0x" FMT_X64 "\t", i, regs[i]);
		printf("reg[%d]\t= 0x" FMT_X64 "\t", i + 1, regs[i + 1]);
		printf("reg[%d]\t= 0x" FMT_X64 "\t", i + 2, regs[i + 2]);
		printf("reg[%d]\t= 0x" FMT_X64, i + 3, regs[i + 3]);
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
					}
					break;
				}
				case FN_SLTU:{
					regs[rd] = (((uint64_t)regs[ra] < (uint64_t)regs[rb]) ? 1 : 0);
					break;
				}
				case FN_SLT:{
					regs[rd] = (((int64_t)regs[ra] < (int64_t)regs[rb]) ? 1 : 0);
					break;
				}
				case FN_SEQ:{
					regs[rd] = (((uint64_t)regs[ra] == (uint64_t)regs[rb]) ? 1 : 0);
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0202, funct);
					exit(EXIT_FAILURE);
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
					setWord((uint64_t)(regs[ra] + imm), (uint64_t)regs[rb], codeBase, heap, stackBase);
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0204, funct);
					exit(EXIT_FAILURE);
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
					regs[rd] = loadWord((uint64_t)(regs[ra] + imm), codeBase, fileLength, heap, stackBase);
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0205, funct);
					exit(EXIT_FAILURE);
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
				case FN_BLE:{
					if((int64_t)regs[ra] <= (int64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BGT:{
					if((int64_t)regs[ra] > (int64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BGTU:{
					if((uint64_t)regs[ra] > (uint64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BGE:{
					if((int64_t)regs[ra] >= (int64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BGEU:{
					if((uint64_t)regs[ra] >= (uint64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				case FN_BLEU:{
					if((uint64_t)regs[ra] <= (uint64_t)regs[rb]){
						regs[PC] = (regs[PC] - 1) + (uint64_t)imm;
					}
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0206, funct);
					exit(EXIT_FAILURE);
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
					if(!handleSyscall(vm, exit_code, fileLength))
						running = false;
					break;
				}
				case FN_NOP:{
					break;	// funnily enough this is good enough already
				}
				case FN_BREAK:{
					fprintf(stderr, "[BREAK]: Breakpoint hit at PC " FMT_U64 "\n", regs[PC] - 1);
					fprintf(stderr, "Registers:\n");

					for(int i = 0; i < 64; i++){
						fprintf(stderr, "  r%-2d: 0x" FMT_X64 "\n", i, regs[i]);
					}

					fprintf(stderr, "Press enter to continue...\n");
					getchar();
					break;
				}
				default:
					fprintf(stderr, "[FATAL 0x%04X]: Illegal function 0x%02X.\n", 0x0207, funct);
					exit(EXIT_FAILURE);
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
	if(regs[SP] >= ((STACKSIZE)+STACK_ADDR)){
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

__attribute__((hot))	// tell the compiler that this is a hotpath
void run(CortexVM *vm, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code){
	uint64_t  *regs      = vm->regs;
	uint64_t  *codeBase  = vm->codeBase;
	uint64_t  *stackBase = vm->stackBase;
	HeapState *heap      = vm->heap;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
	// =========================================================
	// threaded dispatch: fused (opcode << 8 | funct) -> label
	// table is built once on first call, then sticks around.
	// only base-ISA opcodes live in the table; extensions
	// fall through to do_illegal -> handleExtensionOpcode.
	// =========================================================
	static void *dispatch[65536];
	static bool dispatch_inited = false;

	// fill out the dispatch table
	if(!dispatch_inited){
		for(int i = 0; i < 65536; i++)
			dispatch[i] = &&do_illegal;

		dispatch[(OP_R << 8) | FN_ADDSUB ] = &&l_r_addsub;
		dispatch[(OP_R << 8) | FN_OR] = &&l_r_or;
		dispatch[(OP_R << 8) | FN_XOR] = &&l_r_xor;
		dispatch[(OP_R << 8) | FN_AND] = &&l_r_and;
		dispatch[(OP_R << 8) | FN_SLL] = &&l_r_sll;
		dispatch[(OP_R << 8) | FN_SR] = &&l_r_sr;
		dispatch[(OP_R << 8) | FN_SLTU] = &&l_r_sltu;
		dispatch[(OP_R << 8) | FN_SLT] = &&l_r_slt;
		dispatch[(OP_R << 8) | FN_SEQ] = &&l_r_seq;

		dispatch[(OP_I << 8) | FN_ADDSUB] = &&l_i_addsub;
		dispatch[(OP_I << 8) | FN_OR] = &&l_i_or;
		dispatch[(OP_I << 8) | FN_XOR] = &&l_i_xor;
		dispatch[(OP_I << 8) | FN_AND] = &&l_i_and;
		dispatch[(OP_I << 8) | FN_SLL] = &&l_i_sll;
		dispatch[(OP_I << 8) | FN_SR] = &&l_i_sr;
		dispatch[(OP_I << 8) | FN_JMP] = &&l_i_jmp;

		dispatch[(OP_S << 8) | FN_SW] = &&l_s_sw;
		dispatch[(OP_L << 8) | FN_LW] = &&l_l_lw;

		dispatch[(OP_B << 8) | FN_BEQ] = &&l_b_beq;
		dispatch[(OP_B << 8) | FN_BNE] = &&l_b_bne;
		dispatch[(OP_B << 8) | FN_BLT] = &&l_b_blt;
		dispatch[(OP_B << 8) | FN_BLTU] = &&l_b_bltu;
		dispatch[(OP_B << 8) | FN_BGE] = &&l_b_bge;
		dispatch[(OP_B << 8) | FN_BGT] = &&l_b_bgt;
		dispatch[(OP_B << 8) | FN_BGTU] = &&l_b_bgtu;
		dispatch[(OP_B << 8) | FN_BLE] = &&l_b_ble;
		dispatch[(OP_B << 8) | FN_BLEU] = &&l_b_bleu;
		dispatch[(OP_B << 8) | FN_BGEU] = &&l_b_bgeu;

		dispatch[(OP_SYS << 8) | FN_HALT] = &&l_sys_halt;
		dispatch[(OP_SYS << 8) | FN_SYSCALL] = &&l_sys_syscall;
		dispatch[(OP_SYS << 8) | FN_NOP] = &&l_sys_nop;
		dispatch[(OP_SYS << 8) | FN_BREAK] = &&l_sys_break;

		dispatch_inited = true;
	}

	// ====================================================================
	// pre-decode the entire code section into a parallel array of
	// DecodedInstr records. cached across calls; rebuilt when codeBase
	// or fileLength changes, or when runCacheReset() is called by the
	// persistent VM API to signal that the previous code arena was freed
	// and a new binary loaded (the arena may hand back the same address,
	// so pointer equality alone is not a reliable freshness signal).
	// a sentinel halt sits at decoded[fileLength] so falling off the end
	// of code halts naturally — no per-instruction PC bound check needed
	// in the hot loop.
	// ====================================================================
	if(codeBase != cached_cb || fileLength != cached_len){
		if(decoded_cap < fileLength + 1){
			free(decoded);
			decoded = (DecodedInstr*)malloc((fileLength + 1) * sizeof(DecodedInstr));

			if(!decoded){
				fprintf(stderr, "[FATAL 0x%04X]: Could not allocate decoded instruction cache.\n", 0x3801);
				exit(EXIT_FAILURE);
			}

			decoded_cap = fileLength + 1;
		}

		// decode the entire file
		for(uint64_t i = 0; i < fileLength; i++){
			uint64_t w = codeBase[i];
			uint8_t op = OPCODE(w);
			uint8_t fn = FUNCT(w);
			// key
			uint16_t k = ((uint16_t)op << 8) | (uint8_t)fn;
			// set values in DecodedInstr struct
			decoded[i].handler = dispatch[k];
			decoded[i].opcode = op;
			decoded[i].rd = I_RD(w);
			decoded[i].ra = I_RA(w);
			decoded[i].rb = I_RB(w);
			decoded[i].flags = FLAGS(w);
			decoded[i].raw = w;

			// pre-sign-extend the immediate per opcode shape.
			// L uses L_IMM (different layout), S/B use S_IMM, I/R use I_IMM.
			switch(op){
				case OP_S:
				case OP_B:{
					decoded[i].imm = SIGN_EXT36(S_IMM(w));
					break;
				}
				case OP_L:{
					decoded[i].imm = SIGN_EXT36(L_IMM(w));
					break;
				}
				case OP_I:
				case OP_R:
				default:{
					decoded[i].imm = SIGN_EXT32(I_IMM(w));
					break;
				}
			}
		}

		// sentinel halt (running off the end of code lands here)
		decoded[fileLength].handler = &&l_sys_halt;
		decoded[fileLength].opcode = OP_SYS;
		decoded[fileLength].rd = decoded[fileLength].ra = decoded[fileLength].rb = 0;
		decoded[fileLength].flags = 0;
		decoded[fileLength].imm = 0;
		decoded[fileLength].raw = 0;

		cached_cb = codeBase;
		cached_len = fileLength;
	}

	// ====================================================================
	// hot path locals. restrict tells the optimizer these don't alias
	// each other or the dispatch/decoded arrays, so it can keep things
	// in CPU registers across the indirect goto.
	// ====================================================================
	uint64_t  * restrict r  = regs;
	uint64_t  * restrict cb = codeBase;
	uint64_t  * restrict sb = stackBase;
	HeapState * restrict hp = heap;

	uint64_t pc = r[PC];
	const uint64_t code_end = fileLength;
	DecodedInstr *d;

	// NEXT(): enforce r0=0 (one store), fetch decoded, jump. No PC bound
	// check, no SP overflow check — those are off the hot path now.
	#define NEXT() do{ \
		r[ZERO] = 0; \
		d = &decoded[pc++]; \
		goto *d->handler; \
	} while(0)

	// PC may be at or beyond code_end on entry (fresh program, prior halt)
	if(pc > code_end){
		r[PC] = pc;
		return;
	}
	d = &decoded[pc++];
	goto *d->handler;

	// NOTE: IF ANY OF THE FOLLOWING IS CONFUSING JUST LOOK AT THE run() FUNCTION
	// it should be basically identical in form; just as labels not a switch.
	// the run function is slightly more clear with variable names and the such
	
	// ============== R-type ==============
	l_r_addsub:{
		uint64_t a = r[d->ra], b = r[d->rb];
		uint8_t f = d->flags;

		if(f == 0)
			r[d->rd] = a + b;
		else if(f == 1)
			r[d->rd] = a - b;
		else{
			fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x0208, f);
			exit(EXIT_FAILURE);
		}

		NEXT();
	}
	l_r_or:{
		r[d->rd] = r[d->ra] | r[d->rb];
		NEXT();
	}
	l_r_xor:{
		r[d->rd] = r[d->ra] ^ r[d->rb];
		NEXT();
	}
	l_r_and:{
		r[d->rd] = r[d->ra] & r[d->rb];
		NEXT();
	}
	l_r_sll:{
		r[d->rd] = r[d->ra] << (r[d->rb] & 0x3F);
		NEXT();
	}
	l_r_sr:{
		uint64_t a = r[d->ra];
		uint64_t s = r[d->rb] & 0x3F;
		uint8_t f = d->flags;

		if(f == 0)
			r[d->rd] = a >> s;
		else if(f == 1)
			r[d->rd] = (uint64_t)((int64_t)a >> s);
		else{
			fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x0209, f);
			exit(EXIT_FAILURE);
		}

		NEXT();
	}
	l_r_sltu:{r[d->rd] = (r[d->ra] <  r[d->rb]); NEXT(); }
	l_r_slt:{ r[d->rd] = ((int64_t)r[d->ra] < (int64_t)r[d->rb]); NEXT(); }
	l_r_seq:{ r[d->rd] = (r[d->ra] == r[d->rb]); NEXT(); }

	// ============== I-type ==============
	l_i_addsub:{
		uint64_t a = r[d->ra]; uint64_t imm = (uint64_t)d->imm;
		uint8_t f = d->flags;

		if(f == 0)
			r[d->rd] = a + imm;
		else if(f == 1)
			r[d->rd] = a - imm;
		else{
			fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x020A, f);
			exit(EXIT_FAILURE);
		}

		NEXT();
	}
	l_i_or:{
		r[d->rd] = r[d->ra] | (uint64_t)d->imm;
		NEXT();
	}
	l_i_xor:{
		r[d->rd] = r[d->ra] ^ (uint64_t)d->imm;
		NEXT();
	}
	l_i_and:{
		r[d->rd] = r[d->ra] & (uint64_t)d->imm;
		NEXT();
	}
	l_i_sll:{
		r[d->rd] = r[d->ra] << ((uint64_t)d->imm & 0x3F);
		NEXT();
	}
	l_i_sr:{
		uint64_t a = r[d->ra];
		uint64_t s = (uint64_t)d->imm & 0x3F;
		uint8_t f = d->flags;

		if(f == 0)
			r[d->rd] = a >> s;
		else if(f == 1)
			r[d->rd] = (uint64_t)((int64_t)a >> s);
		else{
			fprintf(stderr, "[FATAL 0x%04X]: Illegal flags 0x%01X.\n", 0x020B, f);
			exit(EXIT_FAILURE);
		}

		NEXT();
	}
	l_i_jmp:{
		uint64_t target = r[d->ra] + (uint64_t)d->imm;
		uint64_t link = pc;	// pc already points past this instr

		// clamp out-of-bounds targets to the sentinel
		if(target > code_end)
			target = code_end;

		r[d->rd] = link;
		pc = target;

		NEXT();
	}

	// ============== S-type / L-type ==============
	l_s_sw:{
		setWord(r[d->ra] + (uint64_t)d->imm, r[d->rb], cb, hp, sb);
		NEXT();
	}
	l_l_lw:{
		r[d->rd] = loadWord(r[d->ra] + (uint64_t)d->imm, cb, fileLength, hp, sb);
		NEXT();
	}

	// ============== B-type (branchless via cmov) ==============
	// pc was already incremented in NEXT(); branch target = (pc-1) + imm.
	// Computing both target and fallthrough then selecting via ?: lets the
	// compiler emit cmov instead of a conditional branch.
	#define BR(COND) do{ \
		uint64_t a = r[d->ra], b = r[d->rb]; \
		uint64_t target = (pc - 1) + (uint64_t)d->imm; \
		\
		if(target > code_end) \
			target = code_end; \
		pc = (COND) ? target : pc; \
		\
		NEXT(); \
	} while(0)

	l_b_beq:
		BR(a == b);
	l_b_bne:
		BR(a != b);
	l_b_blt:
		BR((int64_t)a <  (int64_t)b);
	l_b_bltu:
		BR(a <  b);
	l_b_bge:
		BR((int64_t)a >= (int64_t)b);
	l_b_bgt:
		BR((int64_t)a >  (int64_t)b);
	l_b_bgtu:
		BR(a >  b);
	l_b_ble:
		BR((int64_t)a <= (int64_t)b);
	l_b_bleu:
		BR(a <= b);
	l_b_bgeu:
		BR(a >= b);
	#undef BR

	// ============== SYS ==============
	l_sys_halt:
		r[PC] = pc;
		return;
	l_sys_syscall:{
		// sync PC out so syscalls that read it see the right value;
		// reload after in case a syscall modified it.
		r[PC] = pc;
		if(!handleSyscall(vm, exit_code, fileLength)){
			return;
		}
		pc = r[PC];

		NEXT();
	}
	l_sys_nop:
		NEXT();
	l_sys_break:{
		// dump all registers to stderr
		fprintf(stderr, "[BREAK]: Breakpoint hit at PC " FMT_U64 "\n", pc - 1);
		fprintf(stderr, "Registers:\n");
		for(int i = 0; i < 64; i++)
			fprintf(stderr, "  r%-2d: 0x" FMT_X64 "\n", i, r[i]);
		fprintf(stderr, "Press enter to continue...\n");
		getchar();

		NEXT();
	}

	// ============== fallback (extensions or illegal) ==============
	do_illegal:{
		// sync PC so the extension handler sees a consistent state
		r[PC] = pc;
		uint8_t opcode = d->opcode;

		if(!handleExtensionOpcode(opcode, extensions, d->raw, regs)){
			fprintf(stderr, "[FATAL 0x%04X]: Illegal opcode 0x%02X.\n", 0x0201, opcode);
			exit(EXIT_FAILURE);
		}
		pc = r[PC];
		
		NEXT();
	}

	#undef NEXT
#pragma GCC diagnostic pop
#else
	// portable fallback for compilers without labels-as-values
	while(step(vm, fileLength, extensions, exit_code));
#endif
}

void heapDestroy(CortexVM *vm){
	heapStateDestroy(vm->heap);
}

// 	.:
