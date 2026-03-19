#include <stdio.h>
#include <stdlib.h>

#include "../include/utils.h"

// ===========
// Basic Utils
// ===========

// Read a file into a buffer
char *readFile(const char *path, size_t *outLen){
	FILE *file = fopen(path, "rb");	// read as binary
	if(!file){
		fprintf(stderr, "[FATAL 0x%04X]: Could not open file at \"%s\".\n", 0x0101, path);
		exit(EXIT_FAILURE);
	}

	// get the file size
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	// allocate size of buffer
	char *buffer = malloc(sizeof(char) * size + 1);
	if(!buffer){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate buffer of size %zu bytes.\n", 0x0102, size + 1);
		exit(EXIT_FAILURE);
	}

	if(fread(buffer, 1, size, file) != (size_t)size){
		free(buffer);
		fclose(file);

		fprintf(stderr, "[FATAL 0x%04X]: Read in buffer does not match size of file.\n", 0x0103);
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
		fprintf(stderr, "[FATAL 0x%04X]: Could not open file at \"%s\".\n", 0x0111, path);
		exit(EXIT_FAILURE);
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if(size % 8 != 0){
		fclose(file);
		fprintf(stderr, "[FATAL 0x%04X]: File size %ld is not a multiple of 8 bytes.\n", 0x0114, size);
		exit(EXIT_FAILURE);
	}

	size_t wordCount = size / 8;
	uint64_t *buffer = malloc(wordCount * sizeof(uint64_t));
	if(!buffer){
		fprintf(stderr, "[FATAL 0x%04X]: Not enough memory to allocate buffer of size %zu bytes.\n", 0x0112, wordCount * 8);
		exit(EXIT_FAILURE);
	}

	uint8_t *raw = (uint8_t *)buffer;
	if(fread(raw, 1, size, file) != (size_t)size){
		free(buffer);
		fclose(file);
		fprintf(stderr, "[FATAL 0x%04X]: Read in buffer does not match size of file.\n", 0x0113);
		exit(EXIT_FAILURE);
	}

	// reconstruct each word from bytes in big-endian order
	for(size_t i = 0; i < wordCount; i++){
		uint8_t *b = raw + i * 8;
		buffer[i] = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48)
		           | ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32)
		           | ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16)
		           | ((uint64_t)b[6] <<  8) | ((uint64_t)b[7]);
	}

	fclose(file);
	if(outWordCount)
		*outWordCount = wordCount;
	return buffer;
}
