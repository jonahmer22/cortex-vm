#ifndef CORTEX_VM_H
#define CORTEX_VM_H

#include <stdint.h>
#include <stddef.h>

// assemble source text and write binary to outputPath (defaults to "a.out" if NULL)
// returns heap-allocated word buffer; caller must free(); binary[1] == word count
uint64_t *cortexAssemble(const char *source, const char *outputPath);

// assemble source text and run it, returns exit code
int cortexExecSource(const char *source);

// run a pre-assembled binary (word buffer + word count), returns exit code
int cortexExecBinary(const uint64_t *binary, size_t wordCount);

#endif

//  .:
