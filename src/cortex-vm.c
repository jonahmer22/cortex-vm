#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../include/cortex-vm.h"
#include "../include/core.h"
#include "../include/assembler.h"
#include "../include/header.h"
#include "../include/defs.h"
#include "../deps/arena/arena.h"

int cortexExecSource(const char *source){
    // assemble the source
    uint64_t *buff = assemble(source, "", true);
    if(buff == NULL){
        fprintf(stderr, "[FATAL 0x%04X]: Assembly of passed source failed.\n", 0xFAF0);
        return EXIT_FAILURE;
    }

    // buff[1] is the internal fileLength encoded into the binary, just use that here
    int exit_code = cortexExecBinary(buff, buff[1]);

    free(buff);

    return exit_code;
}

int cortexExecBinary(const uint64_t *binary, size_t wordCount){
    // ================
	// parse the header
	// ================

	// create empty variables for things to be parsed into
	uint64_t magic = 0;
	uint16_t version = 0;
	uint64_t fileLength = 0;
	uint64_t offset = 0;
	uint64_t extensions = 0;
	uint64_t dataOffset = 0;

	headerParse(&magic, &version, &fileLength, &offset, &extensions, &dataOffset, binary);

	// ===================
	// validate the header
	// ===================

    // we just pass in fileLength here twice since we don't actually care
	headerValidate(&magic, &version, &wordCount, &fileLength, &offset, &extensions, &dataOffset);

    // ===========
	// init the vm
	// ===========
	
	// initialize the memory arenas
	Arena *code = arenaLocalInit();
	// Arena *heap = arenaLocalInit();
	Arena *stack = arenaLocalInit();

	uint64_t *codeBase = arenaLocalAlloc(code, sizeof(uint64_t) * (fileLength - HEADER_LEN));
	// TODO: when the heap is implemented initialize it here, for now do nothing
	uint64_t *stackBase = arenaLocalAlloc(stack, STACKSIZE);

	// move code into the code section
	for(size_t i = HEADER_LEN; i < fileLength; i++){
		codeBase[i - HEADER_LEN] = binary[i];
	}
	fileLength -= HEADER_LEN;

	// initialize all registers to 0s
	uint64_t regs[64] = {0};
	regs[1] = offset - HEADER_LEN;		// set PC to entry point relative to code base
	regs[2] = 0x0008000000000000;	// set sp to the stack base (top 16 bits is 0x0008)
	
	// exit_code
	uint64_t exit_code = 0;

	// fetch, decode, and execute code
	run(regs, codeBase,/* heapBase,*/ stackBase, fileLength, extensions, &exit_code);

	// free all the memory for the vm and exit with no errors
	arenaLocalDestroy(code);
	// arenaLocalDestroy(heap);
	arenaLocalDestroy(stack);

    // it's the callers duty to free binary

	return (int)exit_code;
}

//  .:
