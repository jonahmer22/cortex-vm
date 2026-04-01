#ifndef CORTEX_VM_H
#define CORTEX_VM_H

#include <stdint.h>
#include <stddef.h>

// assemble source text and run it, returns exit code
int cortexExecSource(const char *source);

// run a pre-assembled binary (word buffer + word count), returns exit code
int cortexExecBinary(const uint64_t *binary, size_t wordCount);

#endif

//  .:
