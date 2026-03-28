#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/assembler.h"
#include "../include/defs.h"
#include "../include/list.h"

#define ENCODE_OPCODE(op)	((uint64_t)((op) & 0xff) << 56)
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

char *head = NULL;
size_t line = 0;

void skipSep(void){
	while(*head == ' ' || *head == '\t' || *head == ',')
		head++;
}

bool cmpChars(char *head, const char *cmp, size_t len){
	for(size_t i = 0; i < len; i++){
		if(toupper(*(head+i)) != toupper(*(cmp+i)))
			return false;
	}

	return true;
}

void getOpcodeFunct(uint8_t *opcode, uint8_t *funct, uint8_t *flags){
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
					break;
				}
			}
			break;
		}
		case 'O':
		case 'o':{
			head++;
			// should be or or ori
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
							break;
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
				default:{
					goto OP_FAILURE;
					break;
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
					// should be blt or bltu
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
					else
						goto OP_FAILURE;
					break;
				}
				default:{
					goto OP_FAILURE;
					break;
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
		default:{
			OP_FAILURE:
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent opcode on line %zu.\n", 0x0301, line);
			exit(EXIT_FAILURE);
			break;
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
			// should be a litteral reg number or ra
			if(cmpChars(head, "a", 1)){
				head++;
				// is ra
				*r = RA;
				break;
			}
			uint8_t reg = parseRegNum();
			if(!(reg < 64))
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
			if(!(reg < 14))
				goto REG_FAILURE;
			
			*r = reg + S0;
			break;
		}
		case 'A':
		case 'a':{
			head++;
			// should be an arg reg number
			uint8_t reg = parseRegNum();
			if(!(reg < 14))
				goto REG_FAILURE;
			
			*r = reg + A0;
			break;
		}
		case 'T':
		case 't':{
			head++;
			// should be a temp reg number
			uint8_t reg = parseRegNum();
			if(!(reg < 32))
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
			break;
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
			break;
		}
		default:{
			REG_FAILURE:
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent register on line %zu.\n", 0x0303, line);
			exit(EXIT_FAILURE);
			break;
		}
	}
}
void getImm(int64_t *val){
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
				while(ishexnumber(*head)){
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
				break;
			}
		}
	}
	else{
		// dealing with decimal
		temp = strtoll(head, &head, 10);
	}

	*val = neg ? -temp : temp;
}

uint64_t *assemble(char *sbuff){
	// implement assembler
	// - must check for opcode then parse based off of the char value of that opcode.
	// - use macros for destination indeces and addresses so easy to change in the future
	// - return an array of words representing the code

	// init the word list
	List *list = listInit();

	// init the header and add to word list
	// add signature + magic + version number
	listAppend(list, (0x2E3A000000000000 + 0x0000434F52540000 + VERSION));
	listAppend(list, 0);	// this is temporary and will be set later (for file length)
	listAppend(list, 0);	// this is temporary and will be set later (for entry point)
	listAppend(list, 0);	// this is temporary and will be set later (for extension flags)

	head = sbuff;
	while(*head != '\0'){
		skipSep();
		// keep track of lines
		if(*head == '\n'){
			head++;
			line++;
			continue;
		}

		// TODO: (FUTURE) check for stuff like to put in the data section

		// TODO: (FUTURE) check for labels and store them in a table along with their positions

		// the current word we are assembling
		uint64_t word = 0;
		
		// use functions to make the propper word
		uint8_t opcode, funct, flags;
		opcode = funct = flags = 0;
		getOpcodeFunct(&opcode, &funct, &flags);
		// consume white space or comma or both
		skipSep();
		switch(opcode){
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
			case OP_I:{
				uint8_t ra, rd;
				int64_t imm;
				ra = rd = imm = 0;
				getReg(&rd);
				skipSep();
				getReg(&ra);
				skipSep();
				getImm(&imm);

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
				getImm(&imm);

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
				getImm(&imm);

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_RD(rd) | ENCODE_L_IMM(imm);
				break;
			}
			case OP_B:{
				uint8_t ra, rb;
				int64_t imm;
				ra = rb = imm = 0;
				getReg(&ra);
				skipSep();
				getReg(&rb);
				skipSep();
				getImm(&imm);	// TODO: (FUTURE) should be parsing a label instead and then computing jump address to encode

				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct) | ENCODE_RA(ra) | ENCODE_B_IMM(imm) | ENCODE_RB(rb);
				break;
			}
			case OP_SYS:{
				word = ENCODE_OPCODE(opcode) | ENCODE_FUNCT(funct);
				break;
			}
			default:{
				fprintf(stderr, "[FATAL 0x%04X]: This should not be possible. If you are seeing this please contact a developer.\n[DEBUG]: Illegal opcode: 0x%02X\n[DEBUG]: Illegal funct: 0x%02X\n[DEBUG]: Illegal flags: 0x%01X\n", 0x0302, opcode, funct, flags);
				exit(EXIT_FAILURE);
				break;
			}
		}

		#ifdef DEBUG
		printf("[DEBUG}: instruction encoded:\t0x%016llX\n", word);
		#endif

		// add the word to the words list
		listAppend(list, word);
	}
	listSet(list, 1, list->len);	// set the file length (should be just the length of the list)
	listSet(list, 2, 4);	// TODO: set the entry point as just the first instruction temporarily
	listSet(list, 3, 0);	// don't worry about flags for right now.

	// return the raw array of the word list
	uint64_t *words = listToArray(list);
	listDestroy(list);

	return words;
}
