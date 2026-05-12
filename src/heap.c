#include <stdlib.h>

#include "../include/heap.h"

HeapState *heapStateCreate(){
    HeapState *heap = malloc(sizeof(HeapState));

    heap->base = NULL;
    heap->cap = 0;
    heap->used = 0;

    return heap;
}

void heapStateDestroy(HeapState *heap){
    if(heap != NULL){
        free(heap->base);
        free(heap);
    }
}