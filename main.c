#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"

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

// Read a file into a uint64_t word array (little endian, zero padded to 8-byte boundary)
uint64_t *readFileWords(const char *path, size_t *outWordCount){
	FILE *file = fopen(path, "rb");
	if(!file){
		fprintf(stderr, "[FATAL 0x%04X]: Could not open file at \"%s\".\n", 0x0021, path);
		exit(EXIT_FAILURE);
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if(size % 8 != 0){
		fclose(file);
		fprintf(stderr, "[FATAL 0x%04X]: File size %ld is not a multiple of 8 bytes.\n", 0x0024, size);
		exit(EXIT_FAILURE);
	}

	size_t wordCount = size / 8;
	uint64_t *buffer = malloc(wordCount * sizeof(uint64_t));
	if(!buffer){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate buffer of size %zu bytes.\n", 0x0022, wordCount * 8);
		exit(EXIT_FAILURE);
	}

	if(fread(buffer, 1, size, file) != (size_t)size){
		free(buffer);
		fclose(file);
		fprintf(stderr, "[FATAL 0x%04X]: Read in buffer does not match size of file.\n", 0x0023);
		exit(EXIT_FAILURE);
	}

	fclose(file);
	if(outWordCount)
		*outWordCount = wordCount;
	return buffer;
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
	for(int i = 255; i >= 0; i--){
		printf("%d ", t[i]);
	}
	printf("\n");

	arenaDestroy();

	printf("%s\n%lu\n", buff, size);
	free(buff);

	size_t wordCount = 0;
	uint64_t *hexBuff = readFileWords(argv[1], &wordCount);

	printf("\nRaw 64-bit hex:\n");
	for(size_t i = 0; i < wordCount; i++){
		printf("0x%016llX\n", hexBuff[i]);
	}

	free(hexBuff);
	return 0;
}

//	.:
