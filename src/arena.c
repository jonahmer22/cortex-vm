// =====
// Arena
// =====

#include "arena.h"

// arena uses error numbers 0x0100

static MemBlock *memBlockInit(size_t size){
	size_t allocSize = (size <= BLOCK_SIZE ? BLOCK_SIZE : size);

	MemBlock *block = malloc(sizeof(MemBlock));
	if(!block){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate arena block struct.\n", 0x0101);
		exit(EXIT_FAILURE);
	}
	block->head = block->buffer = malloc(sizeof(char) * allocSize);
	if(!block->head){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate arena block of %ld bytes.\n", 0x0102, allocSize);
		exit(EXIT_FAILURE);
	}
	block->used = 0;
	block->size = allocSize;
	block->nextBlock = NULL;
	
	return block;
}

static MemBlock *memBlockAppend(MemBlock *curr, size_t size){
	MemBlock *temp = memBlockInit(size);

	curr->nextBlock = temp;
	
	return temp;
}

static MemBlock *memBlockDestroy(MemBlock *curr){
	free(curr->buffer);
	curr->buffer = NULL;
	curr->head = NULL;
	curr->size = 0;
	curr->used = 0;

	MemBlock *temp = curr->nextBlock;
	curr->nextBlock = NULL;
	free(curr);

	return temp;
}

void arenaInit(void){
	arena = malloc(sizeof(Arena));
	if(!arena){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate arena struct.\n", 0x0103);
		exit(EXIT_FAILURE);
	}
	
	arena->blocks = NULL;
	arena->head = NULL;
	arena->numBytes = 0;
	arena->numBlocks = 0;
}

void *arenaAlloc(size_t size){
	if(arena->blocks == NULL){
		arena->head = arena->blocks = memBlockInit(size);
		arena->numBlocks++;
	}

	uintptr_t p = (uintptr_t)arena->head->head;
	size_t a = alignof(max_align_t);
	uintptr_t aligned = (p + (a - 1)) & ~(uintptr_t)(a - 1);

	size_t pad = (size_t)(aligned - p);
	size_t need = pad + size;


	size_t available = arena->head->size - arena->head->used;
	if(available < need){
		arena->head = memBlockAppend(arena->head, need);
		arena->numBlocks++;
		p = (uintptr_t)arena->head->head;
		aligned = (p + (a - 1)) & ~(uintptr_t)(a - 1);
		pad = (size_t)(aligned - p);
	}

	arena->head->head += pad;
	arena->head->used += pad;

	void *retAddr = arena->head->head;
	arena->head->head += size;
	arena->head->used += size;
	arena->numBytes += need;

	return retAddr;
}

void arenaDestroy(void){
	if(arena->numBlocks != 0){
		for(MemBlock *temp = memBlockDestroy(arena->blocks); temp != NULL; temp = memBlockDestroy(temp));
	}
	free(arena);
}
