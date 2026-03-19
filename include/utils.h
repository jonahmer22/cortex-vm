#ifndef UTILS_H
#define UTILS_H

// read a file into a char buffer and returns a pointer to that
char *readFile(const char *path, size_t *outLen);
// reads a file into a uint64_t buffer and returns a pointer to that
uint64_t *readFileWords(const char *path, size_t *outWordCount);

#endif
