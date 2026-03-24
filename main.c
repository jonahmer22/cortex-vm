#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "include/core.h"
#include "include/utils.h"
#include "include/header.h"
#include "include/defs.h"

// ==================
// start of execution
// ==================

int main(int argc, char **argv){
	if(argc < 2){
		fprintf(stdout, "[USAGE]: ./cortex-vm <./path/to/file> [flags]\n");
		exit(EXIT_FAILURE);
	}

	// eventually need to parse arguements to check for flags like -a to assemble
	// for right now all this does is to execute as if it was a binary
	
	// Read in the file and get it's size
	size_t fileSize = 0;
	uint64_t *buff = readFileWords(argv[1], &fileSize);

	// ================
	// parse the header
	// ================

	// create empty variables for things to be parsed into
	uint64_t magic = 0;
	uint16_t version = 0;
	uint64_t fileLength = 0;
	uint64_t offset = 0;
	uint64_t extensions = 0;

	headerParse(&magic, &version, &fileLength, &offset, &extensions, buff);

	// TODO: remove this in the future
	// for testing purposes print the values parsed from the header
	printf("Header Contents:\n");
	printf("magic:\t\t0x%016llX\n", magic);
	printf("version:\t0x%016X\n", version);
	printf("fileLength:\t0x%016llX\n", fileLength);
	printf("offset:\t0x%016llX\n", offset);
	printf("extensions:\t%064llb\n", extensions);

	// ===================
	// validate the header
	// ===================

	headerValidate(&magic, &version, &fileSize, &fileLength, &offset, &extensions, buff);

	// ===========
	// init the vm
	// ===========
	
	// initialize the memory arenas
	Arena *code = arenaLocalInit();
	Arena *heap = arenaLocalInit();
	Arena *stack = arenaLocalInit();

	uint64_t *codeBase = arenaLocalAlloc(code, sizeof(uint64_t) * (fileLength - 4));
	// TODO: when the heap is implemented initialize it here, for now do nothing
	uint64_t *stackBase = arenaLocalAlloc(stack, STACKSIZE);

	// move code into the code section
	for(size_t i = 4; i < fileLength; i++){
		codeBase[i - 4] = buff[i];
	}
	fileLength -= 4;

	// this should no longer be needed
	free(buff);

	// initialize all registers to 0s
	uint64_t regs[64] = {0};
	regs[1] = offset - 4;		// set PC to offset - 4
	regs[2] = 0x0008000000000000;	// set sp to the stack base (top 16 bits is 0x0008)
	
	// TODO: in the future when extensions exist initialize them here.
	
	// fetch, decode, and execute code
	run(regs, codeBase,/* heapBase,*/ stackBase, fileLength);

	// free all the memory for the vm and exit with no errors
	arenaLocalDestroy(code);
	arenaLocalDestroy(heap);
	arenaLocalDestroy(stack);
	return EXIT_SUCCESS;
}

//	.:
