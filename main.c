#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"

#define VERSION 0x0000000000000001

#define STACKSIZE (1024*1024*sizeof(uint64_t))

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

	uint8_t *raw = (uint8_t *)buffer;
	if(fread(raw, 1, size, file) != (size_t)size){
		free(buffer);
		fclose(file);
		fprintf(stderr, "[FATAL 0x%04X]: Read in buffer does not match size of file.\n", 0x0023);
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

// ==================
// start of execution
// ==================

int main(int argc, char **argv){
	if(argc < 2){
		fprintf(stdout, "[USAGE]: ./cortex-vm <./path/to/file> [flags]\n");
		exit(EXIT_FAILURE);
	}

	// eventually need to parse arguements to check for flags like -a to assemble
	// for right now all this does is to execute as if it was a binary
	
	// Read in the file and get it's size
	size_t fileSize = 0;
	uint64_t *buff = readFileWords(argv[1], &fileSize);

	// print the raw values of the binary for testing
	for(size_t i = 0; i < fileSize; i++){
		printf("0x%016llX\n", (unsigned long long)buff[i]);
	}

	// ================
	// parse the header
	// ================

	printf("Header Contents:\n");

	// parse the magic number
	uint64_t magic = buff[0];
	magic >>= 16;
	printf("magic:\t\t0x%016llX\n", magic);
	// parse the version number
	uint16_t version = buff[0] & 0xFFFF;
	printf("version:\t0x%016X\n", version);
	// parse the fileLength
	uint64_t fileLength = buff[1];
	printf("fileLength:\t0x%016llX\n", fileLength);
	// parse the offset
	uint64_t offset = buff[2];
	printf("fileLength:\t0x%016llX\n", offset);
	// parse the extension flags
	uint64_t extensions = buff[3];
	printf("extensions:\t%064llb\n", extensions);

	// ===================
	// validate the header
	// ===================

	// make sure the version number is not greater than implementation version
	if(version > VERSION){
		fprintf(stderr, "[VERSION 0x%04X]: Version number 0x%04X reported by binary is greater than Cortex-VM implementation version 0x%04X.\n", VERSION, version, VERSION);
		free(buff);
		exit(EXIT_FAILURE);
	}
	// make sure that the magic number is the same should be ".:CORT" in ascii
	if(magic != 0x00002E3A434F5254){
		fprintf(stderr, "[HEADER FORMATTING]: Binary header is not propperly formatted.\n");
		free(buff);
		exit(EXIT_FAILURE);
	}
	// make sure that the fileSize and fileLength match
	if(fileSize != fileLength){
		fprintf(stderr, "[FILE LENGTH]: A file size of %zu was loaded, while the encoded binary specifies a size of %llu.\n", fileSize, fileLength);
		free(buff);
		exit(EXIT_FAILURE);
	}
	// make sure that the offset is at least 4
	if(offset < 4){
		fprintf(stderr, "[ENTRY POINT]: The specified entry point of %llu is within the header, minimum entry point is 4.\n", offset);
		free(buff);
		exit(EXIT_FAILURE);
	}
	// check the extension flags, for version 1 they should all be 0
	if(extensions != (extensions & 0x0000000000000000)){
		fprintf(stderr, "[NONEXISTENT EXTENSIONS]: Non-existent extensions were specified in the binary header, please ensure extensions are installed and you are using the propper version.\n");
		free(buff);
		exit(EXIT_FAILURE);
	}
		
	// ===========
	// init the vm
	// ===========
	
	// initialize the memory arenas
	Arena *code = arenaLocalInit();
	Arena *heap = arenaLocalInit();
	Arena *stack = arenaLocalInit();

	uint64_t *codeBase = arenaLocalAlloc(code, sizeof(uint64_t) * (fileLength - 4));
	// TODO: when the heap is implemented initialize it here, for now do nothing
	uint64_t *stackBase = arenaLocalAlloc(stack, STACKSIZE);

	// move code into the code section
	for(size_t i = 4; i < fileLength; i++){
		codeBase[i - 4] = buff[i];
	}

	// this should no longer be needed
	free(buff);

	// initialize all registers to 0s
	uint64_t regs[64] = {0};
	regs[1] = offset - 4;		// set PC to offset - 4
	regs[2] = 0x0008000000000000;	// set sp to the stack base (top 16 bits is 0x0008)
	
	// TODO: in the future when extensions exist initialize them here.
	
	// TODO: execute code, should probably be in function and not in main.

	// free all the memory for the vm and exit with no errors
	arenaLocalDestroy(code);
	arenaLocalDestroy(heap);
	arenaLocalDestroy(stack);
	return 0;
}

//	.:
