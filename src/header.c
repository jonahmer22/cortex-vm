#include <stdio.h>
#include <stdint.h>

#include "../include/defs.h"
#include "../include/header.h"

void headerParse(uint64_t *magic, uint16_t *version, uint64_t *fileLength, uint64_t *offset, uint64_t *extensions, const uint64_t *buff){
	// ================
	// parse the header
	// ================

	// parse the magic number
	*magic = buff[0];
	*magic >>= 16;
	// parse the version number
	*version = buff[0] & 0xFFFF;
	// parse the fileLength
	*fileLength = buff[1];
	// parse the offset
	*offset = buff[2];
	// parse the extension flags
	*extensions = buff[3];

	#ifdef DEBUG
	// for testing purposes print the values parsed from the header
	printf("[DEBUG]: Header Contents:\n");
	printf("[DEBUG]: magic:\t\t0x%016llX\n", *magic);
	printf("[DEBUG]: version:\t0x%016X\n", *version);
	printf("[DEBUG]: fileLength:\t0x%016llX\n", *fileLength);
	printf("[DEBUG]: offset:\t0x%016llX\n", *offset);
	printf("[DEBUG]: extensions:\t0b");
	for(int i = 63; i >= 0; --i)
		putchar((*extensions >> i) & 1 ? '1' : '0');
	printf(" (FLOAT:%d M:%d)\n", (int)((*extensions & EXT_FLOAT) != 0), (int)((*extensions & EXT_M) != 0));
	#endif
}

void headerValidate(uint64_t *magic, uint16_t *version, size_t *fileSize, uint64_t *fileLength, uint64_t *offset, uint64_t *extensions){
	// ===================
	// validate the header
	// ===================

	// make sure the version number is not greater than implementation version
	if(*version > VERSION){
		fprintf(stderr, "[VERSION 0x%04X]: Version number 0x%04X reported by binary is greater than Cortex-VM implementation version 0x%04X.\n", VERSION, *version, VERSION);
		exit(EXIT_FAILURE);
	}
	// make sure that the magic number is the same should be ".:CORT" in ascii
	if(*magic != 0x00002E3A434F5254){
		fprintf(stderr, "[HEADER FORMATTING]: Binary header is not propperly formatted.\n");
		exit(EXIT_FAILURE);
	}
	// make sure that the fileSize and fileLength match
	if(*fileSize != *fileLength){
		fprintf(stderr, "[FILE LENGTH]: A file size of %zu was loaded, while the encoded binary specifies a size of %llu.\n", *fileSize, *fileLength);
		exit(EXIT_FAILURE);
	}
	// make sure that the offset is at least 4
	if(*offset < 4){
		fprintf(stderr, "[ENTRY POINT]: The specified entry point of %llu is within the header, minimum entry point is 4.\n", *offset);
		exit(EXIT_FAILURE);
	}
	// check the extension flags, for version 1 they should all be 0
	uint64_t validExts = EXT_FLOAT | EXT_M;
	if(*extensions != (*extensions & validExts)){
		fprintf(stderr, "[NONEXISTENT EXTENSIONS]: Non-existent extensions were specified in the binary header, please ensure extensions are installed and you are using the propper version.\n");
		exit(EXIT_FAILURE);
	}
}

// 	.:
