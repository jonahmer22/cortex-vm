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

// TODO: make a file reader for pure binary list of uint64_t words instead of as a char buffer like the one above

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
	return 0;
}

//	.:
