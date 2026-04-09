#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

// read a file into a char buffer and returns a pointer to that
char *readFile(const char *path, size_t *outLen);
// reads a file into a uint64_t buffer and returns a pointer to that
uint64_t *readFileWords(const char *path, size_t *outWordCount);
// write a char buffer to a file (creates or overwrites)
void writeFile(const char *path, char *sbuff, size_t len);
// write a uint64_t word array to a file in big-endian byte order (creates or overwrites)
void writeFileWords(const char *path, uint64_t *buff, size_t wordCount);

#endif

//  .:
