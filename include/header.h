#ifndef HEADER_H
#define HEADER_H

#include <stdint.h>
#include <stdlib.h>

#define VERSION 0x0000000000000001

// parses a header into multiple variables
void headerParse(uint64_t *magic, uint16_t *version, uint64_t *fileLength, uint64_t *offset, uint64_t *extensions, uint64_t *buff);
// validates that a header has values that actually make sense
void headerValidate(uint64_t *magic, uint16_t *version, size_t *fileSize, uint64_t *fileLength, uint64_t *offset, uint64_t *extensions, uint64_t *buff);

#endif
