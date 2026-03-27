#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "cliargs.h"
#include "include/core.h"
#include "include/utils.h"
#include "include/header.h"
#include "include/defs.h"

// ==================
// start of execution
// ==================

int main(int argc, char **argv){
	// set up cliargs parameters before parse
	cliargsStrict();
	cliargsSetVersion("Cortex-VM v0.0.1");
	cliargsSetDescription("Cortex-VM is a general purpose, extensible virtual machine built around a custom virtual ISA.\nThis program may be used to execute, assemble, or disassemble binaries designed for the Cortex-ISA.");
	
	// add all different args
	cliargsRegister("assemble", 'a', "Assembles a program at the given path");
	cliargsRegister("disassemble", 'd', "Disassembles a program at the given path");
	cliargsRegister("visual", 'v', "Starts the UI");
	cliargsRegister("output", 'o', "Designates a path for the output file");

	// need to parse arguements to check for flags like -a to assemble or others
	cliargsParse(argc, argv);
	if(!cliargsValid()){
		fprintf(stderr, "[FATAL 0x%04X]: Invalid arguments: %s.\n", 0x0001, cliargsError());
		return EXIT_FAILURE;
	}

	// get the path from the args (should always just be the subcommand)
	// this will be to:
	// - a binary to be executed
	// - a binary to be disassembled
	// - a source file to be assembled
	char *path = NULL;	// path to file
	char *sbuff = NULL;	// contents of source file being assembled etc
	size_t outLen = 0;
	uint64_t *buff = NULL;	// code buffer
	
	// check for flags to either assemble, dissassemble, open visual mode, set output path
	if(cliargsFlag("assemble", 'a') || cliargsArg("assemble", 'a') != NULL){
		if(path == NULL)
			path = cliargsArg("assemble", 'a');
		#ifdef DEBUG
		printf("[DEBUG]: assemble flag enabled, path: %s\n", path);
		#endif

		sbuff = readFile(path, &outLen);
		// TODO: call assemble the source; put into buff

	}
	if(cliargsFlag("disassemble", 'd') || cliargsArg("disassemble", 'd') != NULL){
		if(path == NULL)
			path = cliargsArg("disassemble", 'd');
		#ifdef DEBUG
		printf("[DEBUG]: disassemble flag enabled, path: %s\n", path);
		#endif

		buff = readFileWords(path, &outLen);
		// TODO: call disassemble the words; put into sbuff

	}
	if(cliargsFlag("visual", 'v')){
		#ifdef DEBUG
		printf("[DEBUG]: visual flag enabled\n");
		#endif
	}
	char *outputPath = cliargsArg("output", 'o');
	if(outputPath != NULL){
		#ifdef DEBUG
		printf("[DEBUG]: output flag enabled, path: %s\n", outputPath);
		#endif
	}
	
	// ===================================
	// default execution of binary at path
	// ===================================
	
	// get the path from the args (should always just be the subcommand)
	if(path == NULL)
		path = cliargsSubcommand();
	if(path == NULL){
		cliargsPrintHelp();
		fprintf(stderr, "[FATAL 0x%04X]: Invalid or missing path. A path must be given to a file to either; assemble, disassemble, or execute.\n", 0x0002);
		return EXIT_FAILURE;
	}
	
	// Read in the file and get it's size
	size_t fileSize = 0;
	if(buff == NULL)
		buff = readFileWords(path, &fileSize);

	// ================
	// parse the header
	// ================

	// create empty variables for things to be parsed into
	uint64_t magic = 0;
	uint16_t version = 0;
	uint64_t fileLength = 0;
	uint64_t offset = 0;
	uint64_t extensions = 0;

	headerParse(&magic, &version, &fileLength, &offset, &extensions, buff);

	// ===================
	// validate the header
	// ===================

	headerValidate(&magic, &version, &fileSize, &fileLength, &offset, &extensions, buff);

	// ===========
	// init the vm
	// ===========
	
	// initialize the memory arenas
	Arena *code = arenaLocalInit();
	// Arena *heap = arenaLocalInit();
	Arena *stack = arenaLocalInit();

	uint64_t *codeBase = arenaLocalAlloc(code, sizeof(uint64_t) * (fileLength - 4));
	// TODO: when the heap is implemented initialize it here, for now do nothing
	uint64_t *stackBase = arenaLocalAlloc(stack, STACKSIZE);

	// move code into the code section
	for(size_t i = 4; i < fileLength; i++){
		codeBase[i - 4] = buff[i];
	}
	fileLength -= 4;

	// initialize all registers to 0s
	uint64_t regs[64] = {0};
	regs[1] = offset - 4;		// set PC to offset - 4
	regs[2] = 0x0008000000000000;	// set sp to the stack base (top 16 bits is 0x0008)
	
	// TODO: in the future when extensions exist initialize them here.
	
	// fetch, decode, and execute code
	run(regs, codeBase,/* heapBase,*/ stackBase, fileLength);

	// free all the memory for the vm and exit with no errors
	arenaLocalDestroy(code);
	// arenaLocalDestroy(heap);
	arenaLocalDestroy(stack);

	// free the buffers that hold code and source
	// wait until here so that the UI can use both and build later
	free(buff);
	free(sbuff);

	return EXIT_SUCCESS;
}

//	.:
