#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stdbool.h>

void run(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength);
bool step(uint64_t *regs, uint64_t *codeBase,/* uint64_t *heapBase,*/ uint64_t *stackBase, uint64_t fileLength);

#endif
