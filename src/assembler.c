#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../include/assembler.h"
#include "../include/defs.h"

char *head = NULL;
size_t line = 0;

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
			fprintf(stderr, "[FATAL 0x%04X]: Non-existent opcode on line %zu", 0x0301, line);
			exit(EXIT_FAILURE);
			break;
		}
	}
}
void getRa(uint8_t *ra){

}
void getRb(uint8_t *rb){
	
}
void getRd(uint8_t *rd){
	
}
void getI(uint64_t *val){

}

uint64_t *assemble(char *sbuff){
	// TODO: implement assembler
	// - must check for opcode then parse based off of the char value of that opcode.
	// - use macros for destination indeces and addresses so easy to change in the future
	// - return an array of words representing the code
	head = sbuff;
}
