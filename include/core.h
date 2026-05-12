#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "cortex-vm.h"
#include "heap.h"

void run(CortexVM *vm, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code);
bool step(CortexVM *vm, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code);
void heapDestroy(CortexVM *vm);
uint64_t *heapSnapshot(HeapState *heap, uint64_t *out_used);

#endif

//  .:
