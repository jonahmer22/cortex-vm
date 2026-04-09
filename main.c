#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "deps/arena/arena.h"
#include "deps/cliargs/cliargs.h"
#include "include/core.h"
#include "include/utils.h"
#include "include/header.h"
#include "include/defs.h"
#include "include/assembler.h"
#include "include/disassembler.h"

// ==================
// start of execution
// ==================

int main(int argc, char **argv){
	// set up cliargs parameters before parse
	cliargsStrict();
	cliargsSetVersion("Cortex-VM v0.0.1");
	cliargsSetDescription("Cortex-VM is a general purpose, extensible virtual machine built around a custom virtual ISA.\nThis program may be used to execute, assemble, or disassemble binaries designed for the Cortex-ISA.");
	
	// add all different args
	cliargsRegister("output", 'o', "Designates a path for the output file");
	cliargsRegister("assemble", 'a', "Assembles a program at the given path");
	cliargsRegister("disassemble", 'd', "Disassembles a program at the given path");
	cliargsRegister("run", 'r', "Executes a compiled binary immediately after execution (may only be used in conjunction with -d and -a flags)");
	cliargsRegister("no-output", 'n', "Prevents the assembler or disassembler from creating an output file (may only be used in conjunction with -d and -a flags)");
	cliargsRegister("visual", 'v', "Starts the UI");

	// TODO: might be a cool idea, but I don't know how to exactly execute an idea like this
	// cliargsRegister("portable", 'p', "Output binary includes execution engine for portability");

	// need to parse arguments to check for flags like -a to assemble or others
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
	char *outputPath = NULL;	// path to the output file if present
	char *sbuff = NULL;	// contents of source file being assembled etc
	size_t outLen = 0;
	uint64_t *buff = NULL;	// code buffer
	
	// check for output paths before other flags so that the output can be used in assembler and disassembler
	outputPath = cliargsArg("output", 'o');
	if(outputPath != NULL){
		#ifdef DEBUG
		printf("[DEBUG]: output flag enabled, path: %s\n", outputPath);
		#endif
	}
	// check for -r flag (run after assemble/disassemble)
	int runAfter = cliargsFlag("run", 'r');
	// check for the --no-output flag
	int noOutput = cliargsFlag("no-output", 'n');

	// check for flags to either assemble, disassemble, open visual mode
	if(cliargsFlag("assemble", 'a') || cliargsArg("assemble", 'a') != NULL){
		if(path == NULL)
			path = cliargsArg("assemble", 'a');
		if(path == NULL)
			path = cliargsSubcommand();
		#ifdef DEBUG
		printf("[DEBUG]: assemble flag enabled, path: %s\n", path);
		#endif

		sbuff = readFile(path, &outLen);
		// call assemble the source; put into buff and write out to file
		buff = assemble(sbuff, outputPath, noOutput);

		if(!runAfter){
			free(buff);
			free(sbuff);
			return EXIT_SUCCESS;
		}
	}
	if(cliargsFlag("disassemble", 'd') || cliargsArg("disassemble", 'd') != NULL){
		if(path == NULL)
			path = cliargsArg("disassemble", 'd');
		if(path == NULL)
			path = cliargsSubcommand();
		#ifdef DEBUG
		printf("[DEBUG]: disassemble flag enabled, path: %s\n", path);
		#endif

		buff = readFileWords(path, &outLen);
		sbuff = disassemble(buff, outputPath, noOutput);

		if(!runAfter){
			free(buff);
			free(sbuff);
			return EXIT_SUCCESS;
		}
	}
	if(cliargsFlag("visual", 'v') || cliargsArg("visual", 'v') != NULL){
		if(path == NULL)
			path = cliargsArg("visual", 'v');
		#ifdef DEBUG
		printf("[DEBUG]: visual flag enabled\n");
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
	if(buff != NULL)
		fileSize = buff[1];
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
	uint64_t dataOffset = 0;

	headerParse(&magic, &version, &fileLength, &offset, &extensions, &dataOffset, buff);

	// ===================
	// validate the header
	// ===================

	headerValidate(&magic, &version, &fileSize, &fileLength, &offset, &extensions, &dataOffset);

	// ===========
	// init the vm
	// ===========
	
	// initialize the memory arenas
	Arena *code = arenaLocalInit();
	// Arena *heap = arenaLocalInit();
	Arena *stack = arenaLocalInit();

	uint64_t *codeBase = arenaLocalAlloc(code, sizeof(uint64_t) * (fileLength - HEADER_LEN));
	// TODO: when the heap is implemented initialize it here, for now do nothing
	uint64_t *stackBase = arenaLocalAlloc(stack, STACKSIZE);

	// move code into the code section
	for(size_t i = HEADER_LEN; i < fileLength; i++){
		codeBase[i - HEADER_LEN] = buff[i];
	}
	fileLength -= HEADER_LEN;

	// initialize all registers to 0s
	uint64_t regs[64] = {0};
	regs[1] = offset - HEADER_LEN;		// set PC to entry point relative to code base
	regs[2] = 0x0008000000000000;	// set sp to the stack base (top 16 bits is 0x0008)
	
	// exit_code
	uint64_t exit_code = 0;

	// fetch, decode, and execute code
	run(regs, codeBase,/* heapBase,*/ stackBase, fileLength, extensions, &exit_code);

	// free all the memory for the vm and exit with no errors
	arenaLocalDestroy(code);
	// arenaLocalDestroy(heap);
	arenaLocalDestroy(stack);

	// free the buffers that hold code and source
	// wait until here so that the UI can use both and build later
	free(buff);
	free(sbuff);

	return (uint8_t)exit_code;
}

//	.:
