#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../include/assembler.h"
#include "../include/utils.h"
#include "../include/defs.h"
#include "../include/list.h"

#define ENCODE_OPCODE(op)	((uint64_t)((op) & 0xff) << 56)
#define DECODE_OPCODE(w)		(((w) >> 56) & 0xff)
#define ENCODE_FUNCT(fn)	((uint64_t)((fn) & 0xff) << 48)
#define ENCODE_RA(ra)		((uint64_t)((ra) & 0x3f) << 42)
#define ENCODE_RD(rd)		((uint64_t)((rd) & 0x3f) << 36)
#define ENCODE_RB(rb)		((uint64_t)((rb) & 0x3f) << 30)
#define ENCODE_FLAGS(fl)	((uint64_t)((fl) & 0xf)  << 26)
// I-type: imm[31:26] at word bits 35:30, imm[25:0] at word bits 25:0
#define ENCODE_I_IMM(imm)	(((((uint64_t)(imm) >> 26) & 0x3f) << 30) | ((uint64_t)(imm) & 0x3ffffff))
// S-type and B-type: imm[35:30] at word bits 41:36, imm[29:0] at word bits 29:0
#define ENCODE_S_IMM(imm)	(((((uint64_t)(imm) >> 30) & 0x3f) << 36) | ((uint64_t)(imm) & 0x3fffffff))
#define ENCODE_L_IMM(imm)	((uint64_t)(imm) & 0xfffffffff)
#define ENCODE_B_IMM(imm)	ENCODE_S_IMM(imm)

// registry of labels and their pc addresses
LabelList *labelsRegistry;
// labels to be patched
LabelList *labelsPatches;

const char *head = NULL;
size_t line = 0;

bool skipSep(void){
	bool skipped = false;
	while(*head == ' ' || *head == '\t' || *head == ','){
		head++;
		skipped = true;
	}

	return skipped;
}

bool skipComments(void){
	// skip comments
	if(*head == ';'){
		while(*head != '\n' && *head != '\0'){
			head++;
		}
		return true;
	}
	return false;
}

bool cmpChars(const char *head, const char *cmp, size_t len){
	for(size_t i = 0; i < len; i++){
		if(toupper(*(head+i)) != toupper(*(cmp+i)))
			return false;
	}

	return true;
}

void getData(List *list);

void getOpcodeFunct(uint8_t *opcode, uint8_t *funct, uint8_t *flags, uint64_t pc, List *list, uint64_t *extensions, uint64_t *pDataOffset){
	// might be the start of the .data section
	if(*head == '.'){
		head++;
		if(cmpChars(head, "data", 4)){
			head += 4;
			skipSep();
			*opcode = OP_DATA;
			// capture data section start BEFORE any data words are appended
			*pDataOffset = list->len;
			// we are now healing with the .data section this must be at the end of the file
			while(*head != '\0'){	// parse until the end of the file
				if(*head == '\n'){
					head++;
					line++;
					continue;
				}
				skipSep();
				if(skipComments())
					continue;

				// check for label
				const char *peek = head;
				while(isalnum(*peek) || *peek == '_')
					peek++;
				if(*peek == ':'){
					labelListAppend(labelsRegistry, head, peek, list->len);
					head = peek + 1;
					continue;
				}
				
				// get any value;
				// - list of numbers (array: decimal, octal, binary, hex)
				// - any char value
				// - a string which is to be stored like an array; each char has to have its own uint64_t since we only work in words
				getData(list);
			}
		}
		else{
			fprintf(stderr, "[FATAL 0x%04X]: line beginning with \'.\' but does not contain a section.\n", 0x030D);
			exit(EXIT_FAILURE);
		}
		return;
	}

	// skip comments
	if(skipComments()){
		*opcode = OP_LABEL;
		return;
	}

	// might be a label
	const char *peek = head;
	while(isalnum(*peek) || *peek == '_')
		peek++;
	if(*peek == ':'){
		// label
		labelListAppend(labelsRegistry, head, peek, pc);
		head = peek + 1;

		*opcode = OP_LABEL;
		return;
	}

	// make sure to set the extensions flag if you encounter one
	// might be an opcode
	*flags = 0x00;
	switch(*head){
		case 'A':
		case 'a':{
			head++;
			switch(*head){
				case 'D':
				case 'd':{
					head++;
					// should be addi or add
					if(cmpChars(head, "di", 2)){
						head += 2;
						// is addi
						*opcode = OP_I;
						*funct = FN_ADDSUB;
						*flags = 0x00;
					}
					else if(cmpChars(head, "d", 1)){
						head++;
						// is add
						*opcode = OP_R;
						*funct = FN_ADDSUB;
						*flags = 0x00;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'N':
				case 'n':{
					head++;
					// should be and or andi
					if(cmpChars(head, "di", 2)){
						head += 2;
						// is andi
						*opcode = OP_I;
						*funct = FN_AND;
					}
					else if(cmpChars(head, "d", 1)){
						head++;
						// is and
						*opcode = OP_R;
						*funct = FN_AND;
					}
					else
						goto OP_FAILURE;
					break;
				}
				default:{
					goto OP_FAILURE;
				}
			}
			break;
		}
		case 'O':
		case 'o':{
			head++;
			// should be or | ori
			if(cmpChars(head, "ri", 2)){
				head += 2;
				// is ori
				*opcode = OP_I;
				*funct = FN_OR;
			}
			else if(cmpChars(head, "r", 1)){
				head++;
				// is or
				*opcode = OP_R;
				*funct = FN_OR;
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'X':
		case 'x':{
			head++;
			// should be xor or xori
			if(cmpChars(head, "ori", 3)){
				head += 3;
				// is xori
				*opcode = OP_I;
				*funct = FN_XOR;
			}
			else if(cmpChars(head, "or", 2)){
				head += 2;
				// is xor
				*opcode = OP_R;
				*funct = FN_XOR;
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'S':
		case 's':{
			head++;
			switch(*head){
				case 'U':
				case 'u':{
					head++;
					// should be sub or subi
					if(cmpChars(head, "bi", 2)){
						head += 2;
						// is subi
						*opcode = OP_I;
						*funct = FN_ADDSUB;
						*flags = 0x01;
					}
					else if(cmpChars(head, "b", 1)){
						head++;
						// is sub
						*opcode = OP_R;
						*funct = FN_ADDSUB;
						*flags = 0x01;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'L':
				case 'l':{
					head++;
					// should be sll or slli
					if(cmpChars(head, "li", 2)){
						head += 2;
						// is slli
						*opcode = OP_I;
						*funct = FN_SLL;
					}
					else if(cmpChars(head, "l", 1)){
						head++;
						// is sll
						*opcode = OP_R;
						*funct = FN_SLL;
					}
					else if(cmpChars(head, "tu", 2)){
						head += 2;
						// is sltu
						*opcode = OP_R;
						*funct = FN_SLTU;
					}
					else if(cmpChars(head, "t", 1)){
						head++;
						// is slt
						*opcode = OP_R;
						*funct = FN_SLT;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'R':
				case 'r':{
					head++;
					// is either srl, srli, sra, or srai
					switch(*head){
						case 'L':
						case 'l':{
							// either srl or srli
							if(cmpChars(head, "li", 2)){
								head += 2;
								// is srli
								*opcode = OP_I;
								*funct = FN_SR;
								*flags = 0x00;
							}
							else if(cmpChars(head, "l", 1)){
								head++;
								// is srl
								*opcode = OP_R;
								*funct = FN_SR;
								*flags = 0x00;
							}
							else
								goto OP_FAILURE;
							break;
						}
						case 'A':
						case 'a':{
							// either sra or srai
							if(cmpChars(head, "ai", 2)){
								head += 2;
								// is srai
								*opcode = OP_I;
								*funct = FN_SR;
								*flags = 0x01;
							}
							else if(cmpChars(head, "a", 1)){
								head++;
								// is sra
								*opcode = OP_R;
								*funct = FN_SR;
								*flags = 0x01;
							}
							else
								goto OP_FAILURE;
							break;
						}
						default:{
							goto OP_FAILURE;
						}
					}
					break;
				}
				case 'W':
				case 'w':{
					head++;
					// is sw
					*opcode = OP_S;
					*funct = FN_SW;
					break;
				}
				case 'Y':
				case 'y':{
					head++;
					// should be syscall
					if(cmpChars(head, "scall", 5)){
						head += 5;
						// is syscall
						*opcode = OP_SYS;
						*funct = FN_SYSCALL;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'E':
				case 'e':{
					head++;
					if(cmpChars(head, "q", 1)){
						head++;
						// is seq
						*opcode = OP_R;
						*funct = FN_SEQ;
					}
					else
						goto OP_FAILURE;
					break;
				}
				default:{
					goto OP_FAILURE;
				}
			}
			break;
		}
		case 'J':
		case 'j':{
			head++;
			// should be jmp
			if(cmpChars(head, "mp", 2)){
				head += 2;
				// is jmp
				*opcode = OP_I;
				*funct = FN_JMP;
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'L':
		case 'l':{
			head++;
			// should be lw
			if(cmpChars(head, "w", 1)){
				head++;
				// is lw
				*opcode = OP_L;
				*funct = FN_LW;
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'B':
		case 'b':{
			head++;
			switch(*head){
				case 'R':
				case 'r':{
					head++;
					// should be break
					if(cmpChars(head, "eak", 3)){
						head += 3;
						// is break
						*opcode = OP_SYS;
						*funct = FN_BREAK;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'E':
				case 'e':{
					head++;
					// should be beq
					if(cmpChars(head, "q", 1)){
						head++;
						// is beq
						*opcode = OP_B;
						*funct = FN_BEQ;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'N':
				case 'n':{
					head++;
					// should be bne
					if(cmpChars(head, "e", 1)){
						head++;
						// is bne
						*opcode = OP_B;
						*funct = FN_BNE;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'L':
				case 'l':{
					head++;
					// should be blt or bltu or ble now
					if(cmpChars(head, "tu", 2)){
						head += 2;
						// is bltu
						*opcode = OP_B;
						*funct = FN_BLTU;
					}
					else if(cmpChars(head, "t", 1)){
						head++;
						// is blt
						*opcode = OP_B;
						*funct = FN_BLT;
					}
					else if(cmpChars(head, "eu", 2)){
						head += 2;
						// is bleu
						*opcode = OP_B;
						*funct = FN_BLEU;
					}
					else if(cmpChars(head, "e", 1)){
						head++;
						// is ble
						*opcode = OP_B;
						*funct = FN_BLE;
					}
					else
						goto OP_FAILURE;
					break;
				}
				case 'G':
				case 'g':{
					head++;
					// could be bgt, bgtu, or bge
					if(cmpChars(head, "tu", 2)){
						head += 2;
						// is bgtu
						*opcode = OP_B;
						*funct = FN_BGTU;
					}
					else if(cmpChars(head, "t", 1)){
						head++;
						// is bgt
						*opcode = OP_B;
						*funct = FN_BGT;
					}
					else if(cmpChars(head, "eu", 2)){
						head += 2;
						// is bgeu
						*opcode = OP_B;
						*funct = FN_BGEU;
					}
					else if(cmpChars(head, "e", 1)){
						head++;
						*opcode = OP_B;
						*funct = FN_BGE;
					}
					else
						goto OP_FAILURE;
					break;
				}
				default:{
					goto OP_FAILURE;
				}
			}
			break;
		}
		case 'H':
		case 'h':{
			head++;
			// should be halt
			if(cmpChars(head, "alt", 3)){
				head += 3;
				// is halt
				*opcode = OP_SYS;
				*funct = FN_HALT;
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'N':
		case 'n':{
			head++;
			// should be nop
			if(cmpChars(head, "op", 2)){
				head += 2;
				// is nop
				*opcode = OP_SYS;
				*funct = FN_NOP;
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'M':
		case 'm':{
			head++;
			// can be mul, muli, mulh, mulhu
			if(cmpChars(head, "ul", 2)){
				head += 2;
				*extensions |= EXT_M;
				// we have a mul instruction
				if(cmpChars(head, "hu", 2)){
					head += 2;
					// we have mulhu
					*opcode = OP_MR;
					*funct = FN_MULHU;
				}
				else if(cmpChars(head, "h", 1)){
					head++;
					// we hve mulh
					*opcode = OP_MR;
					*funct = FN_MULH;
				}
				else if(cmpChars(head, "i", 1)){
					head++;
					// we have muli
					*opcode = OP_MI;
					*funct = FN_MUL;
				}
				else{
					if(skipSep()){
						// we have mul
						*opcode = OP_MR;
						*funct = FN_MUL;
					}
					else
						goto OP_FAILURE;
				}
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'D':
		case 'd':{
			head++;
			// can be div, divi, divu, divui
			if(cmpChars(head, "iv", 2)){
				head += 2;
				*extensions |= EXT_M;
				// we have a div instruction
				if(cmpChars(head, "ui", 2)){
					head += 2;
					// we have divui
					*opcode = OP_MI;
					*funct = FN_DIVU;
				}
				else if(cmpChars(head, "u", 1)){
					head++;
					// we hve divu
					*opcode = OP_MR;
					*funct = FN_DIVU;
				}
				else if(cmpChars(head, "i", 1)){
					head++;
					// we have divi
					*opcode = OP_MI;
					*funct = FN_DIV;
				}
				else{
					if(skipSep()){
						// we have div
						*opcode = OP_MR;
						*funct = FN_DIV;
					}
					else
						goto OP_FAILURE;
				}
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'R':
		case 'r':{
			head++;
			// can be rem, remi, remu, remui
			if(cmpChars(head, "em", 2)){
				head += 2;
				*extensions |= EXT_M;
				// we have a rem instruction
				if(cmpChars(head, "ui", 2)){
					head += 2;
					// we have remui
					*opcode = OP_MI;
					*funct = FN_REMU;
				}
				else if(cmpChars(head, "u", 1)){
					head++;
					// we hve remu
					*opcode = OP_MR;
					*funct = FN_REMU;
				}
				else if(cmpChars(head, "i", 1)){
					head++;
					// we have remi
					*opcode = OP_MI;
					*funct = FN_REM;
				}
				else{
					if(skipSep()){
						// we have rem
						*opcode = OP_MR;
						*funct = FN_REM;
					}
					else
						goto OP_FAILURE;
				}
			}
			else
				goto OP_FAILURE;
			break;
		}
		case 'F':
		case 'f':{
			head++;
			*extensions |= EXT_FLOAT;
			// might be an f extension opcode
			switch(*head){
				case 'A':
				case 'a':{
					head++;
					// could be fadd, faddi, fabs
					if(cmpChars(head, "ddi", 3)){
						head += 3;
						// dealing with faddi
						*opcode = OP_FI;
						*funct = FN_FADD;
						break;
					}
					else if(cmpChars(head, "dd", 2)){
						head += 2;
						// dealing with fadd
						*opcode = OP_FR;
						*funct = FN_FADD;
						break;
					}
					else if(cmpChars(head, "bs", 2)){
						head += 2;
						// dealing with fabs
						*opcode = OP_FI;
						*funct = FN_FABS;
						break;
					}
					else
						goto OP_FAILURE;
				}
				case 'S':
				case 's':{
					head++;
					// could be fsub, fsubi, fsqrt
					if(cmpChars(head, "ubi", 3)){
						head += 3;
						// dealing with fsubi
						*opcode = OP_FI;
						*funct = FN_FSUB;
						break;
					}
					else if(cmpChars(head, "ub", 2)){
						head += 2;
						// dealing with fsub
						*opcode = OP_FR;
						*funct = FN_FSUB;
						break;
					}
					else if(cmpChars(head, "qrt", 3)){
						head += 3;
						// dealing with fsqrt
						*opcode = OP_FI;	// NOTE: for all 1 reg arg values we are going to reuse an I type but in Core just not decode the imm
						*funct = FN_FSQRT;
						break;
					}
					else
						goto OP_FAILURE;
				}
				case 'N':
				case 'n':{
					head++;
					// could be a fneg
					if(cmpChars(head, "eg", 2)){
						head += 2;
						// dealing with fneg
						*opcode = OP_FI;
						*funct = FN_FSUB;
						*flags = 0x01;
						break;
					}
					else
						goto OP_FAILURE;
				}
				case 'M':
				case 'm':{
					head++;
					// could be a fmul, fmuli
					if(cmpChars(head, "uli", 3)){
						head += 3;
						// dealing with fmuli
						*opcode = OP_FI;
						*funct = FN_FMUL;
						break;
					}
					else if(cmpChars(head, "ul", 2)){
						head += 2;
						// dealing with fmul
						*opcode = OP_FR;
						*funct = FN_FMUL;
						break;
					}
					else
						goto OP_FAILURE;
				}
				case 'D':
				case 'd':{
					head++;
					// could be a fdiv, fdivi
					if(cmpChars(head, "ivi", 3)){
						head += 3;
						// dealing with fdivi
						*opcode = OP_FI;
						*funct = FN_FDIV;
						break;
					}
					else if(cmpChars(head, "iv", 2)){
						head += 2;
						// dealing with fdiv
						*opcode = OP_FR;
						*funct = FN_FDIV;
						break;
					}
					else
						goto OP_FAILURE;
				}
				case 'T':
				case 't':{
					head++;
					// could be ftoi, ftoui
					if(cmpChars(head, "oui", 3)){
						head += 3;
						// dealing with ftoui
						*opcode = OP_FI;
						*funct = FN_FTOUI;
						break;
					}
					else if(cmpChars(head, "oi", 2)){
						head += 2;
						// dealing with ftoi
						*opcode = OP_FI;
						*funct = FN_FTOI;
						break;
					}
					else
						goto OP_FAILURE;
				}
				case 'B':
				case 'b':{
					head++;
					// we should be dealing with a branch of a float
					// can be fblt, fble, fbgt, fbge
					// floats get extra branch conditions, can still use regular beq and bne
					if(cmpChars(head, "lt", 2)){
						head += 2;
						// dealing with fblt
						*opcode = OP_FB;
						*funct = FN_FBLT;
						break;
					}
					else if(cmpChars(head, "le", 2)){
						head += 2;
						// dealing with fble
						*opcode = OP_FB;
						*funct = FN_FBLE;
						break;
					}
					else if(cmpChars(head, "gt", 2)){
						head += 2;
						// dealing with fbgt
						*opcode = OP_FB;
						*funct = FN_FBGT;
						break;
					}
					else if(cmpChars(head, "ge", 2)){
						head += 2;
						// dealing with fbge
						*opcode = OP_FB;
						*funct = FN_FBGE;
						break;
					}
					else
						goto OP_FAILURE;
				}
				default: {
					goto OP_FAILURE;
				}
			}
			break;
		}
		case 'I':
		case 'i':{
			head++;
			*extensions |= EXT_FLOAT;
			// could be an itof
			if(cmpChars(head, "tof", 3)){
				head += 3;
				// dealing with itof
				*opcode = OP_FI;
				*funct = FN_ITOF;
				break;
			}
			else
				goto OP_FAILURE;
		}
		case 'U':
		case 'u':{
			head++;
			*extensions |= EXT_FLOAT;
			// could be an uitof
			if(cmpChars(head, "itof", 4)){
				head += 4;
				// is uitof
				*opcode = OP_FI;
				*funct = FN_UITOF;
				break;
			}
			else
				goto OP_FAILURE;
		}
		default:{
			OP_FAILURE:
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent opcode on line %zu.\n", 0x0301, line);
			exit(EXIT_FAILURE);
		}
	}
}

static uint8_t parseRegNum(void){
	uint8_t val = 0;
	while(*head >= '0' && *head <= '9'){
		val = (uint8_t)(val * 10 + (*head - '0'));
		head++;
	}
	return val;
}
void getReg(uint8_t *r){
	switch(*head){
		case 'R':
		case 'r':{
			head++;
			// should be a literal reg number or ra
			if(cmpChars(head, "a", 1)){
				head++;
				// is ra
				*r = RA;
				break;
			}
			uint8_t reg = parseRegNum();
			if(reg >= 64)
				goto REG_FAILURE;
			
			*r = reg;
			break;
		}
		case 'S':
		case 's':{
			head++;
			// should be a saved reg number or sp
			if(cmpChars(head, "p", 1)){
				head++;
				// is sp
				*r = SP;
				break;
			}
			uint8_t reg = parseRegNum();
			if(reg >= 14)
				goto REG_FAILURE;
			
			*r = reg + S0;
			break;
		}
		case 'A':
		case 'a':{
			head++;
			// should be an arg reg number
			uint8_t reg = parseRegNum();
			if(reg >= 14)
				goto REG_FAILURE;
			
			*r = reg + A0;
			break;
		}
		case 'T':
		case 't':{
			head++;
			// should be a temp reg number
			uint8_t reg = parseRegNum();
			if(reg >= 32)
				goto REG_FAILURE;
			
			*r = reg + T0;
			break;
		}
		case 'Z':
		case 'z':{
			head++;
			// should be zero reg
			if(cmpChars(head, "ero", 3)){
				head += 3;
				// is zero reg
				*r = ZERO;
				break;
			}
			else
				goto REG_FAILURE;
		}
		case 'P':
		case 'p':{
			head++;
			// should be pc reg
			if(cmpChars(head, "c", 1)){
				head++;
				// is pc
				*r = PC;
				break;
			}
			else
				goto REG_FAILURE;
		}
		default:{
			REG_FAILURE:
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent register on line %zu.\n", 0x0303, line);
			exit(EXIT_FAILURE);
		}
	}
}
void getImm(int64_t *val, uint64_t pc){
	bool neg = false;
	if(*head == '-'){
		neg = true;
		head++;
	}

	uint64_t temp = 0;

	if(*head == '\''){
		head++;
		if(*head == '\\'){
			head++;
			switch(*head){
				case 'n':{
					temp = '\n';
					break;
				}
				case 't':{
					temp = '\t';
					break;
				}
				case 'r':{
					temp = '\r';
					break;
				}
				case '0':{
					temp = '\0';
					break;
				}
				case '\\':{
					temp = '\\';
					break;
				}
				case '\'':{
					temp = '\'';
					break;
				}
				default:{
					fprintf(stderr, "[FATAL 0x%04X]: Unknown escape sequence on line %zu.\n", 0x0305, line);
					exit(EXIT_FAILURE);
				}
			}
		}
		else{
			temp = (uint64_t)*head;
		}
		head++;
		if(*head != '\''){
			fprintf(stderr, "[FATAL 0x%04X]: Unterminated char literal on line %zu.\n", 0x0306, line);
			exit(EXIT_FAILURE);
		}
		head++;
	}
	else if(*head == '0' && (toupper(*(head+1)) == 'X' || toupper(*(head+1)) == 'B' || toupper(*(head+1)) == 'O')){
		head++;
		switch(*head){
			case 'X':
			case 'x':{
				head++;
				// dealing with hex
				while(isxdigit(*head)){
					temp <<= 4;
					if(isalpha(*head)){
						temp += (toupper(*head) - 55);
					}
					else{
						temp += (*head - 48);
					}

					head++;
				}
				break;
			}
			case 'B':
			case 'b':{
				head++;
				// dealing with binary
				while((*head) == '1' || (*head) == '0'){
					temp <<= 1;
					temp += (*head - 48);

					head++;
				}
				break;
			}
			case 'O':
			case 'o':{
				head++;
				// dealing with octal
				while(*head >= '0' && *head <= '7'){
					temp <<= 3;
					temp += (*head - 48);
					
					head++;
				}
				break;
			}
			default:{
				fprintf(stderr, "[FATAL 0x%04X]: Not recognized number format on line %zu.\n", 0x0304, line);
				exit(EXIT_FAILURE);
			}
		}
	}
	else if(isalpha(*head)){
		// parse a label
		const char *start = head;
		while(isalnum(*head) || *head == '_')
			head++;
		// now the label should go from start to head

		// add the label to a register of labels to patch back in later
		labelListAppend(labelsPatches, start, head, pc);
	}
	else{
		// check for a decimal place
		const char *peek = head;
		while(isdigit(*peek))
			peek++;
		if(*peek == '.'){
			// we have a float, floats can either be represented by 1. or 1.0 etc
			char *endptr;
			double d = strtod(head, &endptr);
			head = endptr;

			d = neg ? -d : d;
			float f = (float)d;
			uint32_t bits = 0;

			memcpy(&bits, &f, sizeof(bits));
			temp = (uint64_t)bits;
			*val = (int64_t)temp;
			return;
		}
		else{
			// dealing with decimal
			char *endptr;
			temp = strtoll(head, &endptr, 10);
			head = endptr;
		}
	}

	*val = neg ? -(int64_t)temp : (int64_t)temp;
}
void getData(List *list){
	// we are parsing a string
	if(*head == '"'){
		head++;
		while(*head != '"' && *head != '\0'){
			char temp = *head;
			// escape sequences
			if(*head == '\\'){
				head++;
				switch(*head){
					case 'n':{
						temp = '\n';
						break;
					}
					case 't':{
						temp = '\t';
						break;
					}
					case 'r':{
						temp = '\r';
						break;
					}
					case '0':{
						temp = '\0';
						break;
					}
					case '\\':{
						temp = '\\';
						break;
					}
					case '\'':{
						temp = '\'';
						break;
					}
					default:{
						fprintf(stderr, "[FATAL 0x%04X]: Unknown escape sequence on line %zu.\n", 0x0305, line);
						exit(EXIT_FAILURE);
					}
				}
			}
			listAppend(list, (uint64_t)temp);
			head++;
		}
		if(*head != '"'){
			fprintf(stderr, "[FATAL 0x%04X]: Unterminated string literal on line %zu.\n", 0x030E, line);
			exit(EXIT_FAILURE);
		}
		head++;
		listAppend(list, 0);	// null terminator
	}
	else{
		// should be a number
		const char *peek = head;
		if(*peek == '-')
			peek++;
		while(isdigit(*peek))
			peek++;
		if(*peek == '.'){
			char *endptr;
			double d = strtod(head, &endptr);
			head = endptr;
			uint64_t bits;
			memcpy(&bits, &d, sizeof(bits));
			listAppend(list, bits);
		}
		else{
			// some other sort of value, just use getImm() to parse it
			int64_t val = 0;
			getImm(&val, 0);
			listAppend(list, (uint64_t)val);
		}
	}
}

uint64_t *assemble(const char *sbuff, const char *outputPath, int noOutput){
	// implement assembler
	// - must check for opcode then parse based off of the char value of that opcode.
	// - use macros for destination indexes and addresses so easy to change in the future
	// - return an array of words representing the code

	// reset line counter for accurate error messages on each call
	line = 0;

	// create an extensions word to have flags set later
	uint64_t extensions = 0;

	// data offset to be set later, 0 means no data section
	uint64_t dataOffset = 0;

	// init the word list
	List *list = listInit();
	// init label registry
	labelsRegistry = labelListInit();
	// init label patches
	labelsPatches = labelListInit();

	// init the header and add to word list
	// add signature + magic + version number
	listAppend(list, (0x2E3A000000000000 + 0x0000434F52540000 + VERSION));
	listAppend(list, 0);	// this is temporary and will be set later (for file length)
	listAppend(list, 0);	// this is temporary and will be set later (for entry point)
	listAppend(list, 0);	// this is temporary and will be set later (for extension flags)
	listAppend(list, 0);	// this is temporary and will be set later (for .data offset)

	head = sbuff;
	while(*head != '\0'){
		skipSep();
		// keep track of lines
		if(*head == '\n'){
			head++;
			line++;
			continue;
		}

		// the current word we are assembling
		uint64_t word = 0;
		
		// use functions to make the proper word
		uint8_t opcode, funct, flags;
		opcode = funct = flags = 0;
		getOpcodeFunct(&opcode, &funct, &flags, list->len, list, &extensions, &dataOffset);
		// consume white space or comma or both
		skipSep();
		switch(opcode){
			case OP_FR:
			case OP_MR:
			case OP_R:{
				uint8_t ra, rd, rb;
				ra = rd = rb = 0;
				getReg(&rd);
				skipSep();
				getReg(&ra);
				skipSep();
				getReg(&rb);

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_RD(rd) | ENCODE_RB(rb) | ENCODE_FLAGS(flags);
				break;
			}
			case OP_FI:
			case OP_MI:
			case OP_I:{
				uint8_t ra, rd;
				int64_t imm;
				ra = rd = imm = 0;
				getReg(&rd);
				skipSep();
				getReg(&ra);
				if(opcode == OP_FI){
					switch(funct){
						// list of FI functions that don't take an imm value
						case FN_FSQRT:
						case FN_FABS:
						case FN_FTOI:
						case FN_FTOUI:
						case FN_ITOF:
						case FN_UITOF:
							imm = 0;
							break;
						case FN_FSUB:{
							if(flags == 0x01){
								imm = 0;
								break;
							}
							// needs to fall through to default
							__attribute__((fallthrough));	// this gets the compiler to shut up
						}
						default:
							skipSep();
							getImm(&imm, list->len);
							break;
					}
				}
				else{
					skipSep();
					getImm(&imm, list->len);
				}

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_RD(rd) | ENCODE_I_IMM(imm) | ENCODE_FLAGS(flags);
				break;
			}
			case OP_S:{
				uint8_t ra, rb;
				int64_t imm;
				ra = rb = imm = 0;
				getReg(&ra);
				skipSep();
				getReg(&rb);
				skipSep();
				getImm(&imm, list->len);

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_S_IMM(imm) | ENCODE_RB(rb);
				break;
			}
			case OP_L:{
				uint8_t ra, rd;
				int64_t imm;
				ra = rd = imm = 0;
				getReg(&rd);
				skipSep();
				getReg(&ra);
				skipSep();
				getImm(&imm, list->len);

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_RD(rd) | ENCODE_L_IMM(imm);
				break;
			}
			case OP_FB:
			case OP_B:{
				uint8_t ra, rb;
				int64_t imm;
				ra = rb = imm = 0;
				getReg(&ra);
				skipSep();
				getReg(&rb);
				skipSep();
				getImm(&imm, list->len);

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_B_IMM(imm) | ENCODE_RB(rb);
				break;
			}
			case OP_SYS:{
				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct);
				break;
			}
			case OP_LABEL:{
				continue;
			}
			case OP_DATA:{
				// dataOffset was already captured inside getOpcodeFunct before data words were appended
				break;
			}
			default:{
				fprintf(stderr, "[FATAL 0x%04X]: This should not be possible. If you are seeing this please contact a developer.\n[DEBUG]: Illegal opcode: 0x%02X\n[DEBUG]: Illegal funct: 0x%02X\n[DEBUG]: Illegal flags: 0x%01X\n", 0x0302, opcode, funct, flags);
				exit(EXIT_FAILURE);
			}
		}

		#ifdef DEBUG
		printf("[DEBUG]: Instruction encoded:\t0x%016llX\n", word);
		#endif

		// add the word to the words list
		listAppend(list, word);
	}
	// patch in label values
	for(size_t i = 0; i < labelsPatches->len; i++){
		// walk the list of patches; find the appropriate label address to replace with; update the instruction
		LabelNode *p = labelListGet(labelsPatches, i);
		LabelNode *r = labelListFind(labelsRegistry, p->start, p->end);
		if(r == NULL){
			fprintf(stderr, "[FATAL 0x%04X]: Attempting to patch an non-existent label at pc=%zu.\n", 0x030C, p->pc);
			exit(EXIT_FAILURE);
		}

		uint64_t word = listGet(list, p->pc);
		#ifdef DEBUG
		uint64_t temp_word = word;
		#endif

		uint8_t opcode = DECODE_OPCODE(word);

		switch(opcode){
			case OP_FI:
			case OP_MI:
			case OP_I:{
				word |= ENCODE_I_IMM((int64_t)r->pc - HEADER_LEN);
				break;
			}
			case OP_S:{
				word |= ENCODE_S_IMM((int64_t)r->pc - HEADER_LEN);
				break;
			}
			case OP_L:{
				word |= ENCODE_L_IMM((int64_t)r->pc - HEADER_LEN);
				break;
			}
			case OP_FB:
			case OP_B:{
				word |= ENCODE_B_IMM((int64_t)r->pc - (int64_t)p->pc);
				break;
			}
			default:{
				fprintf(stderr, "[FATAL 0x%04X]: Attempting to patch a label to an opcode without imm values.\n", 0x030B);
				exit(EXIT_FAILURE);
			}
		}

		listSet(list, p->pc, word);

		#ifdef DEBUG
		printf("[DEBUG]: Patched instruction at pc=%zu from 0x%016llX to 0x%016llX\n", (p->pc)-HEADER_LEN, temp_word, word);
		#endif
	}

	listSet(list, 1, list->len);	// set the file length (should be just the length of the list)

	const char *tmp = "main";
	LabelNode *entry = labelListFind(labelsRegistry, tmp, tmp + 4);
	if(entry)
		listSet(list, 2, entry->pc);
	else
		listSet(list, 2, HEADER_LEN);	// if not set the first instruction as entry point

	listSet(list, 3, extensions);	// set extension flags, default is 0 so should be all fine if none exist
	listSet(list, 4, dataOffset);	// set the data offset default is 0

	// return the raw array of the word list
	uint64_t *words = listToArray(list);

	// destroy dynamic lists
	listDestroy(list);
	labelListDestroy(labelsRegistry);
	labelListDestroy(labelsPatches);

	// check if we want to disable output
	if(!noOutput){
		// output the assembled binary at a given path or a.cxv
		const char *outPath = outputPath ? outputPath : "a.out";

		writeFileWords(outPath, words, words[1]);
	}
	
	#ifdef DEBUG
	printf("[DEBUG]: Output assembled source to:\t%s\n", outputPath);
	#endif

	return words;
}

// 	.:
