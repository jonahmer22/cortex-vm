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
uint64_t *listToArray(List *list);

#endif
