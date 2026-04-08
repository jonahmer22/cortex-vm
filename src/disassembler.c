#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../include/defs.h"
#include "../include/list.h"
#include "../include/utils.h"
#include "../include/header.h"
#include "../include/disassembler.h"

// =====================
// instruction decoding
// =====================

// field extraction
#define DECODE_OPCODE(w)	(((w) >> 56) & 0xff)
#define DECODE_FUNCT(w)		(((w) >> 48) & 0xff)
#define DECODE_RA(w)		(((w) >> 42) & 0x3f)
#define DECODE_RD(w)		(((w) >> 36) & 0x3f)
#define DECODE_RB(w)		(((w) >> 30) & 0x3f)
#define DECODE_FLAGS(w)		(((w) >> 26) & 0xf)

// immediate reconstruction (raw, unsigned — apply SIGN_EXT as needed)
// I-type:   imm[31:26] stored at word[35:30], imm[25:0] at word[25:0]
#define DECODE_I_IMM(w)		((((w) >> 30) & 0x3f) << 26 | ((w) & 0x3ffffff))
// S/B-type: imm[35:30] stored at word[41:36], imm[29:0] at word[29:0]
#define DECODE_S_IMM(w)		((((w) >> 36) & 0x3f) << 30 | ((w) & 0x3fffffff))
// L-type:   imm[35:0] contiguous at word[35:0]
#define DECODE_L_IMM(w)		((w) & 0xfffffffff)
// B-type uses the same layout as S-type
#define DECODE_B_IMM(w)		DECODE_S_IMM(w)
// SYS-type: lower 48 bits
#define DECODE_SYS_IMM(w)	((w) & 0xffffffffffff)

// sign extension helpers
#define SIGN_EXT32(v)		((int64_t)(int32_t)(v))
#define SIGN_EXT36(v)		((int64_t)(((v) & (1ULL << 35)) ? ((v) | ~((1ULL << 36) - 1)) : (v)))

// ====================
// float format helper
// ====================

// format a double so the assembler's getImm() always treats it as a float:
// - always has a decimal point
// - no scientific notation without a decimal point
static void fmtFloat(char *buf, size_t bufsize, double d){
	snprintf(buf, bufsize, "%.9g", d);
	int hasDot = (strchr(buf, '.') != NULL);
	int hasExp = (strchr(buf, 'e') != NULL || strchr(buf, 'E') != NULL);
	if(!hasDot && !hasExp){
		strncat(buf, ".0", bufsize - strlen(buf) - 1);
	}
	else if(hasExp && !hasDot){
		char *epos = strchr(buf, 'e');
		if(!epos)
			epos = strchr(buf, 'E');
		if(epos){
			size_t elen = strlen(epos);
			if(strlen(buf) + 2 < bufsize){
				memmove(epos + 2, epos, elen + 1);
				epos[0] = '.';
				epos[1] = '0';
			}
		}
	}
}

// ====================
// base ISA
// ====================

void disOpR(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t flags = DECODE_FLAGS(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	uint8_t rb = DECODE_RB(w);

	const char *mn;
	switch(funct){
		case FN_ADDSUB:{
			mn = (flags & 1) ? "sub" : "add";
			break;
		}
		case FN_OR:{
			mn = "or";
			break;
		}
		case FN_XOR:{
			mn = "xor";
			break;
		}
		case FN_AND:{
			mn = "and";
			break;
		}
		case FN_SLL:{
			mn = "sll";
			break;
		}
		case FN_SR:{
			mn = (flags & 1) ? "sra" : "srl";
			break;
		}
		case FN_SLT:{
			mn = "slt";
			break;
		}
		case FN_SLTU:{
			mn = "sltu";
			break;
		}
		case FN_SEQ:{
			mn = "seq";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown R-type funct 0x%02X.\n", 0x0601, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, r%d\n", mn, rd, ra, rb);
}

void disOpI(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t flags = DECODE_FLAGS(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	int64_t imm = SIGN_EXT32(DECODE_I_IMM(w));

	const char *mn;
	switch(funct){
		case FN_ADDSUB:{
			mn = (flags & 1) ? "subi" : "addi";
			break;
		}
		case FN_OR:{
			mn = "ori";
			break;
		}
		case FN_XOR:{
			mn = "xori";
			break;
		}
		case FN_AND:{
			mn = "andi";
			break;
		}
		case FN_SLL:{
			mn = "slli";
			break;
		}
		case FN_SR:{
			mn = (flags & 1) ? "srai" : "srli";
			break;
		}
		case FN_JMP:{
			mn = "jmp";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown I-type funct 0x%02X.\n", 0x0602, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, %lld\n", mn, rd, ra, (long long)imm);
}

void disOpS(char *line, uint64_t w){
	uint8_t ra = DECODE_RA(w);
	uint8_t rb = DECODE_RB(w);
	int64_t imm = SIGN_EXT36(DECODE_S_IMM(w));
	sprintf(line, "\tsw r%d, r%d, %lld\n", ra, rb, (long long)imm);
}

void disOpL(char *line, uint64_t w){
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	int64_t imm = SIGN_EXT36(DECODE_L_IMM(w));
	sprintf(line, "\tlw r%d, r%d, %lld\n", rd, ra, (long long)imm);
}

void disOpB(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rb = DECODE_RB(w);
	int64_t imm = SIGN_EXT36(DECODE_B_IMM(w));

	const char *mn;
	switch(funct){
		case FN_BEQ:{
			mn = "beq";
			break;
		}
		case FN_BNE:{
			mn = "bne";
			break;
		}
		case FN_BLT:{
			mn = "blt";
			break;
		}
		case FN_BLTU:{
			mn = "bltu";
			break;
		}
		case FN_BLE:{
			mn = "ble";
			break;
		}
		case FN_BGT:{
			mn = "bgt";
			break;
		}
		case FN_BGTU:{
			mn = "bgtu";
			break;
		}
		case FN_BGE:{
			mn = "bge";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown B-type funct 0x%02X.\n", 0x0603, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, %lld\n", mn, ra, rb, (long long)imm);
}

void disOpSYS(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);

	const char *mn;
	switch(funct){
		case FN_HALT:{
			mn = "halt";
			break;
		}
		case FN_SYSCALL:{
			mn = "syscall";
			break;
		}
		case FN_NOP:{
			mn = "nop";
			break;
		}
		case FN_BREAK:{
			mn = "break";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown SYS-type funct 0x%02X.\n", 0x0604, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s\n", mn);
}

// ====================
// F extension
// ====================

void disOpFR(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	uint8_t rb = DECODE_RB(w);

	const char *mn;
	switch(funct){
		case FN_FADD:{
			mn = "fadd";
			break;
		}
		case FN_FSUB:{
			mn = "fsub";
			break;
		}
		case FN_FMUL:{
			mn = "fmul";
			break;
		}
		case FN_FDIV:{
			mn = "fdiv";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown FR-type funct 0x%02X.\n", 0x0605, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, r%d\n", mn, rd, ra, rb);
}

void disOpFI(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t flags = DECODE_FLAGS(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	uint32_t immBits = (uint32_t)DECODE_I_IMM(w);
	float immF = 0;
	memcpy(&immF, &immBits, sizeof(immF));
	double imm = (double)immF;

	char fbuf[64];
	fmtFloat(fbuf, sizeof(fbuf), imm);

	switch(funct){
		case FN_FSQRT:{
			sprintf(line, "\tfsqrt r%d, r%d\n", rd, ra);
			break;
		}
		case FN_FABS:{
			sprintf(line, "\tfabs r%d, r%d\n", rd, ra);
			break;
		}
		case FN_FTOI:{
			sprintf(line, "\tftoi r%d, r%d\n", rd, ra);
			break;
		}
		case FN_FTOUI:{
			sprintf(line, "\tftoui r%d, r%d\n", rd, ra);
			break;
		}
		case FN_ITOF:{
			sprintf(line, "\titof r%d, r%d\n", rd, ra);
			break;
		}
		case FN_UITOF:{
			sprintf(line, "\tuitof r%d, r%d\n", rd, ra);
			break;
		}
		case FN_FSUB:{
			if(flags & 1)
				sprintf(line, "\tfneg r%d, r%d\n", rd, ra);
			else
				sprintf(line, "\tfsubi r%d, r%d, %s\n", rd, ra, fbuf);
			break;
		}
		case FN_FADD:{
			sprintf(line, "\tfaddi r%d, r%d, %s\n", rd, ra, fbuf);
			break;
		}
		case FN_FMUL:{
			sprintf(line, "\tfmuli r%d, r%d, %s\n", rd, ra, fbuf);
			break;
		}
		case FN_FDIV:{
			sprintf(line, "\tfdivi r%d, r%d, %s\n", rd, ra, fbuf);
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown FI-type funct 0x%02X.\n", 0x0606, funct);
			exit(EXIT_FAILURE);
		}
	}
}

void disOpFB(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rb = DECODE_RB(w);
	int64_t imm = SIGN_EXT36(DECODE_B_IMM(w));

	const char *mn;
	switch(funct){
		case FN_FBLT:{
			mn = "fblt";
			break;
		}
		case FN_FBLE:{
			mn = "fble";
			break;
		}
		case FN_FBGT:{
			mn = "fbgt";
			break;
		}
		case FN_FBGE:{
			mn = "fbge";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown FB-type funct 0x%02X.\n", 0x0607, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, %lld\n", mn, ra, rb, (long long)imm);
}

// ====================
// M extension
// ====================

void disOpMR(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	uint8_t rb = DECODE_RB(w);

	const char *mn;
	switch(funct){
		case FN_MUL:{
			mn = "mul";
			break;
		}
		case FN_MULH:{
			mn = "mulh";
			break;
		}
		case FN_MULHU:{
			mn = "mulhu";
			break;
		}
		case FN_DIV:{
			mn = "div";
			break;
		}
		case FN_DIVU:{
			mn = "divu";
			break;
		}
		case FN_REM:{
			mn = "rem";
			break;
		}
		case FN_REMU:{
			mn = "remu";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown MR-type funct 0x%02X.\n", 0x0608, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, r%d\n", mn, rd, ra, rb);
}

void disOpMI(char *line, uint64_t w){
	uint8_t funct = DECODE_FUNCT(w);
	uint8_t ra = DECODE_RA(w);
	uint8_t rd = DECODE_RD(w);
	int64_t imm = SIGN_EXT32(DECODE_I_IMM(w));

	const char *mn;
	switch(funct){
		case FN_MUL:{
			mn = "muli";
			break;
		}
		case FN_DIV:{
			mn = "divi";
			break;
		}
		case FN_DIVU:{
			mn = "divui";
			break;
		}
		case FN_REM:{
			mn = "remi";
			break;
		}
		case FN_REMU:{
			mn = "remui";
			break;
		}
		default:{
			fprintf(stderr, "[FATAL 0x%04X]: Unknown MI-type funct 0x%02X.\n", 0x0609, funct);
			exit(EXIT_FAILURE);
		}
	}
	sprintf(line, "\t%s r%d, r%d, %lld\n", mn, rd, ra, (long long)imm);
}

// =============
// disassemble()
// =============

char *disassemble(const uint64_t *buff, const char *outputPath, int noOutput){
	char *sbuff = NULL;
	LineList *list = lineListInit();

	// parse the header
	uint64_t magic = 0;
	uint16_t version = 0;
	uint64_t fileLength = 0;
	uint64_t offset = 0;
	uint64_t extensions = 0;
	uint64_t dataOffset = 0;

	headerParse(&magic, &version, &fileLength, &offset, &extensions, &dataOffset, buff);

	size_t fileSize = fileLength;
	headerValidate(&magic, &version, &fileSize, &fileLength, &offset, &extensions, &dataOffset);

	// stop before .data section if present, otherwise decode to end of file
	uint64_t codeEnd = (dataOffset > 0) ? dataOffset : fileLength;

	for(uint64_t i = HEADER_LEN; i < codeEnd; i++){
		uint64_t word = buff[i];
		char line[1024] = {0};

		// emit main: label at the entry point
		if(i == offset)
			lineListAppend(list, "main:\n");

		uint8_t opcode = DECODE_OPCODE(word);

		switch(opcode){
			case OP_R:{
				disOpR(line, word);
				break;
			}
			case OP_I:{
				disOpI(line, word);
				break;
			}
			case OP_S:{
				disOpS(line, word);
				break;
			}
			case OP_L:{
				disOpL(line, word);
				break;
			}
			case OP_B:{
				disOpB(line, word);
				break;
			}
			case OP_SYS:{
				disOpSYS(line, word);
				break;
			}
			case OP_FR:{
				disOpFR(line, word);
				break;
			}
			case OP_FI:{
				disOpFI(line, word);
				break;
			}
			case OP_FB:{
				disOpFB(line, word);
				break;
			}
			case OP_MR:{
				disOpMR(line, word);
				break;
			}
			case OP_MI:{
				disOpMI(line, word);
				break;
			}
			default:{
				fprintf(stderr, "[FATAL 0x%04X]: Non-existent opcode encountered. Please check your version.\n", 0x0600);
				exit(EXIT_FAILURE);
			}
		}

		lineListAppend(list, line);
	}

	// emit .data section if present
	if(dataOffset > 0){
		lineListAppend(list, ".data\n");
		uint64_t i = dataOffset;
		while(i < fileLength){
			uint64_t word = buff[i];

			// check if this looks like the start of a null-terminated string
			// (printable ASCII or common escapes, terminated by a 0 word)
			int isString = (word > 0 && word < 128);
			if(isString){
				uint64_t j = i;
				while(j < fileLength && buff[j] != 0){
					uint64_t c = buff[j];
					if((c < 32 && c != '\t' && c != '\n' && c != '\r') || c >= 128){
						isString = 0;
						break;
					}
					j++;
				}
				if(isString && j < fileLength && buff[j] == 0){
					char sline[4096] = {0};
					int pos = 0;
					pos += sprintf(sline + pos, "    \"");
					for(uint64_t k = i; k < j; k++){
						char c = (char)buff[k];
						if(c == '\n')
							pos += sprintf(sline + pos, "\\n");
						else if(c == '\t')
							pos += sprintf(sline + pos, "\\t");
						else if(c == '\r')
							pos += sprintf(sline + pos, "\\r");
						else if(c == '\\')
							pos += sprintf(sline + pos, "\\\\");
						else if(c == '"')
							pos += sprintf(sline + pos, "\\\"");
						else
							pos += sprintf(sline + pos, "%c", c);
					}
					sprintf(sline + pos, "\"\n");
					lineListAppend(list, sline);
					i = j + 1;	// consume the null terminator
					continue;
				}
			}

			// fallback: emit as a decimal number (not sure the floats work but whatever)
			char nline[64];
			sprintf(nline, "\t%llu\n", (unsigned long long)word);
			lineListAppend(list, nline);
			i++;
		}
	}

	size_t len = 0;
	sbuff = lineListJoin(list, &len);

	if(!noOutput){
		const char *outPath = outputPath ? outputPath : "out.s";
		writeFile(outPath, sbuff, len);
	}

	lineListDestroy(list);
	return sbuff;
}

//  .:
