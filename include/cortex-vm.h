#ifndef CORTEX_VM_H
#define CORTEX_VM_H

#include <stdint.h>
#include <stdlib.h>

// ==============
// Persistent API
// ==============

// Opaque handle; callers only ever hold a pointer; internals are in cortex-vm.c
typedef struct CortexVM CortexVM;

// Create a new VM context with zeroed registers and fresh stack/heap.
// Returns NULL on allocation failure.
CortexVM *cortexVMCreate(void);

// Assemble `source` and execute it inside `vm`, preserving all VM state afterward.
// Returns the program's exit code (the value passed to SYS_EXIT), or -1 on
// assembly failure. SYS_EXIT does NOT destroy the context; it only ends this run.
int cortexVMExecSource(CortexVM *vm, const char *source);

// Execute a pre-assembled binary (word buffer + word count) inside `vm`.
// Same semantics as cortexVMExecSource regarding state preservation and SYS_EXIT.
int cortexVMExecBinary(CortexVM *vm, const uint64_t *binary, size_t wordCount);

// Free all resources owned by `vm`. Do not use the pointer afterward.
void cortexVMDestroy(CortexVM *vm);

// ================================================
// Old Non-Persistent API (kept for the sake of it)
// ================================================

// assemble source text and write binary to outputPath (defaults to "a.out" if NULL)
// returns heap-allocated word buffer; caller must free(); binary[1] == word count
uint64_t *cortexAssemble(const char *source, const char *outputPath);

// assemble source text and run it, returns exit code
int cortexExecSource(const char *source);

// run a pre-assembled binary (word buffer + word count), returns exit code
int cortexExecBinary(const uint64_t *binary, size_t wordCount);

#endif

//  .:
