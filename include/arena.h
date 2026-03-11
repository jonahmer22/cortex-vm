#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>

#define BLOCK_SIZE (1024)

typedef struct MemBlock{
	char *buffer;
	char *head;

	size_t used;
	size_t size;

	struct MemBlock *nextBlock;
} MemBlock;

typedef struct Arena{
	MemBlock *blocks;
	MemBlock *head;
	
	size_t numBlocks;
	size_t numBytes;
} Arena;

static Arena *arena;	// global arena

void arenaInit(void);
void *arenaAlloc(size_t size);
void arenaDestroy(void);

#endif
