#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdint.h>

// ===============
// data structures
// ===============

typedef struct ListNode{
	uint64_t val;
	struct ListNode *next;
} ListNode;

typedef struct List{
	ListNode *head;
	ListNode *tail;
	size_t len;
} List;

List *listInit(void);
void listDestroy(List *list);
void listAppend(List *list, uint64_t val);
void listSet(List *list, size_t index, uint64_t val);
uint64_t listGet(List *list, size_t index);
uint64_t *listToArray(List *list);

// ==========
// Label List
// ==========

typedef struct LabelNode{
	const char *start;  // pointer into source buffer where label name begins
	const char *end;    // pointer one past the last character of the label name
	size_t pc;          // instruction index associated with this label
	struct LabelNode *next;
} LabelNode;

typedef struct LabelList{
	LabelNode *head;
	LabelNode *tail;
	size_t len;
} LabelList;

LabelList *labelListInit(void);
void labelListDestroy(LabelList *list);
void labelListAppend(LabelList *list, const char *start, const char *end, size_t pc);
// returns the first node whose name matches [start, end), or NULL if not found
LabelNode *labelListFind(LabelList *list, const char *start, const char *end);
// returns the node at the given index
LabelNode *labelListGet(LabelList *list, size_t index);

// ==========
// Line List
// ==========

typedef struct LineNode{
	char *line;             // heap-allocated copy of the line
	struct LineNode *next;
} LineNode;

typedef struct LineList{
	LineNode *head;
	LineNode *tail;
	size_t len;             // number of lines
	size_t totalBytes;      // sum of strlen of all lines (excludes null terminator)
} LineList;

LineList *lineListInit(void);
void lineListDestroy(LineList *list);
void lineListAppend(LineList *list, const char *line);
char *lineListJoin(LineList *list, size_t *outLen);

#endif

// 	.:
