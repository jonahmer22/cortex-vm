#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stdbool.h>

void run(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code);
bool step(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength, uint64_t extensions, uint64_t *exit_code);
void heapDestroy(void);

#endif

//  .:
