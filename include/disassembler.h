#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stdint.h>

// disassembles a compiled binary; returns a pointer to a char buffer that contains the source
char *disassemble(const uint64_t *buff, const char *outputPath, int noOutput);

#endif

//  .:
