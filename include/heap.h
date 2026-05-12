#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

// tracks the state of the heap, who would've guessed
typedef struct HeapState{
    uint64_t *base;

    uint64_t used;
    uint64_t cap;
} HeapState;

#endif