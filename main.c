#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// basic size of memory block for arena
#define MEMBLOCK_SIZE 1024*1024	// 1mb by default

// ===========
// Basic Utils
// ===========

// Read a file into a buffer
char *readFile(const char *path, size_t *outLen){
	FILE *file = fopen(path, "rb");	// read as binary
	if(!file){
		fprintf(stderr, "[FATAL 0x%04X]: Could not open file at \"%s\".\n", 0x0011, path);
		exit(EXIT_FAILURE);
	}

	// get the file size
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	// allocate size of buffer
	char *buffer = malloc(sizeof(char) * size + 1);
	if(!buffer){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate buffer of size %zu bytes.\n", 0x0012, size + 1);
		exit(EXIT_FAILURE);
	}

	if(fread(buffer, 1, size, file) != (size_t)size){
		free(buffer);
		fclose(file);

		fprintf(stderr, "[FATAL 0x%04X]: Read in buffer does not match size of file.\n", 0x0013);
		exit(EXIT_FAILURE);
	}

	buffer[size] = '\0';
	if(outLen)
		*outLen = (size_t)size;

	fclose(file);
	return buffer;
}

// TODO: make a file reader for pure binary list of uint64_t words instead of as a char buffer like the one above

// Basic Arena

typedef struct MemBlock{
	uint8_t *block;	// pointer to the base of the memory block

	uint8_t *head;	// pointer to the next open byte of memory
	
	size_t blockSize;	// holds the total size of the block
	size_t free;		// holds the number of free bytes left in the block


	struct MemBlock *next;
} MemBlock;

MemBlock *memBlockInit(size_t blockSize){
	// allocate the MemBlock struct
	MemBlock *result = malloc(sizeof(MemBlock));
	if(!result){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to initialize MemBlock struct of size %zu bytes.\n", 0x0101, sizeof(MemBlock));
		exit(EXIT_FAILURE);
	}

	// allocate the block of memory in the MemBlock
	result->head = result->block = malloc(sizeof(uint8_t) * blockSize);
	if(!result->head || !result->block){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate block of size %zu bytes in MemBlock.\n", 0x0102, blockSize);
		exit(EXIT_FAILURE);
	}

	// set the amount of free bytes and amount of bytes in the block
	result->blockSize = result->free = blockSize;

	// set next to null
	result->next = NULL;

	// return result
	return result;
}

MemBlock *memBlockDestroy(MemBlock *memBlock){
	free(memBlock->block);
	
	MemBlock *result = memBlock->next;

	// set all pointers to NULL
	memBlock->block = memBlock->head = NULL;
	memBlock->next = NULL;
	// set all values to 0
	memBlock->blockSize = memBlock->free = 0;

	free(memBlock);

	return result;
}

typedef struct Arena{
	MemBlock *head;
	MemBlock *currBlock;

	size_t numBlocks;
} Arena;

static Arena arena;

void arenaInit(void){
	arena.head = arena.currBlock = NULL;
	arena.numBlocks = 0;
}

void arenaDestroy(void){
	// if arena is NULL then reset the global arena
	if(arena.head){
		// if there is a head but not a currBlock then something is deeply wrong, just leak the memory and get out
		if(!arena.currBlock){
			fprintf(stderr, "[FATAL 0x%04X]: Error in Arena configuration, MemBlock head (%p) is present, but currBlock (%p) is not\n.", 0x0103, (void *)arena.head, (void *)arena.currBlock);
			exit(EXIT_FAILURE);
		}

		// free all MemBlocks
		for(MemBlock *temp = memBlockDestroy(arena.head); temp; temp = memBlockDestroy(temp));
	}

	// set everything to 0 and NULL then exit
	arena.head = arena.currBlock = NULL;
	arena.numBlocks = 0;
}

void *arenaAlloc(size_t size){
	// if the size of the allocation is lower than the size of the normal block
	if(size < MEMBLOCK_SIZE)
		size = MEMBLOCK_SIZE;
	else{
		// in the case that it isn't we have to make a new block of the propper size and mark it as entirely full
		arena.currBlock->next = memBlockInit(size);
		arena.currBlock = arena.currBlock->next;

		arena.currBlock->free = 0;
		arena.currBlock->head += arena.currBlock->blockSize;

		return arena.currBlock->block;
	}

	// if there is no memblocks yet
	if(!arena.head)
		arena.head = arena.currBlock = memBlockInit(size);

	if(arena.currBlock->free < size){
		arena.currBlock->next = memBlockInit(MEMBLOCK_SIZE);
		arena.currBlock = arena.currBlock->next;

		arena.currBlock->free -= size;
		arena.currBlock->head += size;

		return arena.currBlock->block;
	}
	
	void *base = arena.currBlock->head;
	arena.currBlock->head += size;
	arena.currBlock->free -= size;

	return base;
}

int main(int argc, char **argv){
	// load a file in
	size_t size = 0;
	char *buff = readFile(argv[1], &size);

	arenaInit();

	int *t = arenaAlloc(sizeof(int) * 256);

	for(int i = 0; i < 256; i++){
		t[i] = 256 - i;
	}
	for(int i = 256; i >= 0; i--){
		printf("%d ", t[i]);
	}
	printf("\n");

	arenaDestroy();

	printf("%s\n%zu\n%p\n", buff, size, (void *)&arena);

	free(buff);
	return 0;
}
