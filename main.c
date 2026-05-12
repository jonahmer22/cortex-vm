#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "deps/arena/arena.h"
#include "deps/cliargs/cliargs.h"
#include "include/core.h"
#include "include/cortex-vm.h"
#include "include/vm_ctx.h"
#include "include/utils.h"
#include "include/header.h"
#include "include/defs.h"
#include "include/assembler.h"
#include "include/disassembler.h"
#include "include/server.h"

// ==================
// start of execution
// ==================

int main(int argc, char **argv){
	// set up cliargs parameters before parse
	cliargsStrict();
	cliargsSetVersion("Cortex-VM v1.2.0");
	cliargsSetDescription("Cortex-VM is a general purpose, extensible virtual machine built around a custom virtual ISA.\nThis program may be used to execute, assemble, or disassemble Cortex-ISA binaries.");
	
	// add all different args
	cliargsRegister("output", 'o', "Designates a path for the output file");
	cliargsRegister("assemble", 'a', "Assembles a program at the given path");
	cliargsRegister("disassemble", 'd', "Disassembles a program at the given path");
	cliargsRegister("run", 'r', "Executes a compiled binary immediately after execution (may only be used in conjunction with -d and -a flags)");
	cliargsRegister("no-output", 'n', "Prevents the assembler or disassembler from creating an output file (may only be used in conjunction with -d and -a flags)");
	cliargsRegister("dump-regs", 'D', "Print all 64 registers as JSON to stderr after execution");
	cliargsRegister("visual", 'V', "Start the visual IDE in the browser (optionally pass a source file path)");

	// TODO: might be a cool idea, but I don't know how to exactly execute an idea like this
	// cliargsRegister("portable", 'p', "Output binary includes execution engine for portability");
	// it'd be like packaging an executable into a .app for MacOS or a .exe for windows

	// need to parse arguments to check for flags like -a to assemble or others
	cliargsParse(argc, argv);
	if(!cliargsValid()){
		fprintf(stderr, "[FATAL 0x%04X]: Invalid arguments: %s.\n", 0x0001, cliargsError());
		return EXIT_FAILURE;
	}

	// -V: launch the visual browser-based IDE
	if(cliargsFlag("visual", 'V') || cliargsArg("visual", 'V') != NULL){
		const char *srcPath = cliargsArg("visual", 'V');
		if(!srcPath)
			srcPath = cliargsSubcommand();
		int port = serverFindPort(7777);
		if(port < 0){
			fprintf(stderr, "[FATAL 0x%04X]: No free port found in range 7777-65535.\n", 0x0003);
			return EXIT_FAILURE;
		}
		serverStart(port, argv[0], srcPath);
		return EXIT_SUCCESS;
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

	// check for flags to either assemble or disassemble
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

	CortexVM *vm = cortexVMCreate();
	uint64_t exit_code = (uint64_t)cortexVMExecBinary(vm, buff, fileSize);

	if(cliargsFlag("dump-regs", 'D')){
		fprintf(stderr, "{\"regs\":[");
		for(int i = 0; i < 64; i++){
			fprintf(stderr, "%llu%s", (unsigned long long)vm->regs[i], i < 63 ? "," : "");
		}
		fprintf(stderr, "]}\n");
	}

	cortexVMDestroy(vm);

	// free the buffers that hold code and source
	free(buff);
	free(sbuff);

	return (uint8_t)exit_code;
}

//	.:
