#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../include/cortex-vm.h"
#include "../include/vm_ctx.h"
#include "../include/core.h"
#include "../include/assembler.h"
#include "../include/header.h"
#include "../include/defs.h"
#include "../include/heap.h"
#include "../deps/arena/arena.h"

// Create a new VM context with zeroed registers and fresh stack/heap.
// Returns NULL on allocation failure.
CortexVM *cortexVMCreate(void){
	CortexVM *vm = malloc(sizeof(CortexVM));

	// zero out the registers
	memset(vm->regs, 0, sizeof(uint64_t) * 64);

	// init the stack and set struct values
	vm->stackArena = arenaLocalInit();
	vm->stackBase = arenaLocalAlloc(vm->stackArena, STACKSIZE);
	vm->regs[2] = 0x0008000000000000;	// set sp to the stack base (top 16 bits is 0x0008)

	// idk what to set this for by default so it's just not gonna be set
	vm->codeArena = NULL;
	vm->codeBase = NULL;

	// set upt the heap
	vm->heap = heapStateCreate();

	return vm;
}

// Assemble `source` and execute it inside `vm`, preserving all VM state afterward.
// Returns the program's exit code (the value passed to SYS_EXIT), or -1 on
// assembly failure. SYS_EXIT does NOT destroy the context; it only ends this run.
int cortexVMExecSource(CortexVM *vm, const char *source){
    // assemble the source
    uint64_t *buff = assemble(source, "", true);
    if(buff == NULL){
        fprintf(stderr, "[FATAL 0x%04X]: Assembly of passed source failed.\n", 0xFAF0);
        return EXIT_FAILURE;
    }

	int exit_code = cortexVMExecBinary(vm, buff, buff[1]);

	free(buff);

	return exit_code;
}

// Execute a pre-assembled binary (word buffer + word count) inside `vm`.
// Same semantics as cortexVMExecSource regarding state preservation and SYS_EXIT.
int cortexVMExecBinary(CortexVM *vm, const uint64_t *binary, size_t wordCount){
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

	if(vm->codeArena != NULL)
		arenaLocalDestroy(vm->codeArena);
	vm->codeArena = arenaLocalInit();
	vm->codeBase = arenaLocalAlloc(vm->codeArena, sizeof(uint64_t) * (fileLength - HEADER_LEN));

	// move code into the code section
	for(size_t i = HEADER_LEN; i < fileLength; i++){
		vm->codeBase[i - HEADER_LEN] = binary[i];
	}
	fileLength -= HEADER_LEN;

	// invalidate the decoded-instruction cache: the arena may have handed
	// codeBase the same address as the previous load, so pointer+length
	// equality cannot distinguish "same binary" from "different binary that
	// happens to be the same size at the same address".
	runCacheReset();

	// reset PC to this binary's entry point
	vm->regs[1] = offset - HEADER_LEN;

	// exit_code
	uint64_t exit_code = 0;

	// fetch, decode, and execute code
	run(vm, fileLength, extensions, &exit_code);

	return (int)exit_code;
}

// Free all resources owned by `vm`. Do not use the pointer afterward.
void cortexVMDestroy(CortexVM *vm){
	arenaLocalDestroy(vm->stackArena);

	if(vm->codeArena != NULL)
		arenaLocalDestroy(vm->codeArena);

	heapStateDestroy(vm->heap);

	free(vm);
}

// ===
// OLD
// ===

uint64_t *cortexAssemble(const char *source, const char *outputPath){
	return assemble(source, outputPath, false);
}

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
	CortexVM *vm = cortexVMCreate();
	int exit_code = cortexVMExecBinary(vm, binary, wordCount);
	cortexVMDestroy(vm);
	return exit_code;
}

//  .:
