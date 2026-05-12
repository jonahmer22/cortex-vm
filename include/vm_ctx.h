#ifndef VM_CTX_H
#define VM_CTX_H

// Internal header: full definition of CortexVM.
// Only include this from .c files that need to inspect the struct fields.
// External callers see only the opaque typedef in cortex-vm.h.

#include <stdint.h>
#include "../deps/arena/arena.h"
#include "heap.h"

struct CortexVM {
	uint64_t regs[64];

	uint64_t *stackBase;
	Arena *stackArena;

	uint64_t *codeBase;
	Arena *codeArena;
	
	HeapState *heap;
};

#endif

//  .:
