#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../include/list.h"

List *listInit(void){
	List *list = malloc(sizeof(List));
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate List.\n", 0x0400);
		exit(EXIT_FAILURE);
	}

	list->head = NULL;
	list->tail = NULL;
	list->len = 0;

	return list;
}

void listDestroy(List *list){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot destroy uninitialized list.\n", 0x0401);
		exit(EXIT_FAILURE);
	}

	ListNode *curr = list->head;
	while(curr){
		ListNode *next = curr->next;
		free(curr);
		curr = next;
	}

	free(list);
}

void listAppend(List *list, uint64_t val){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot append to uninitialized list.\n", 0x0402);
		exit(EXIT_FAILURE);
	}

	ListNode *node = malloc(sizeof(ListNode));
	if(!node){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate ListNode.\n", 0x0403);
		exit(EXIT_FAILURE);
	}

	node->val = val;
	node->next = NULL;

	if(!list->tail){
		list->head = node;
		list->tail = node;
	}
	else{
		list->tail->next = node;
		list->tail = node;
	}

	list->len++;
}

uint64_t listGet(List *list, size_t index){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot index into uninitialized list.\n", 0x0408);
		exit(EXIT_FAILURE);
	}
	if(index >= list->len){
		fprintf(stderr, "[FATAL 0x%04X]: List index %zu out of bounds (len %zu).\n", 0x0409, index, list->len);
		exit(EXIT_FAILURE);
	}

	ListNode *curr = list->head;
	for(size_t i = 0; i < index; i++)
		curr = curr->next;

	return curr->val;
}

void listSet(List *list, size_t index, uint64_t val){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot index into uninitialized list.\n", 0x0404);
		exit(EXIT_FAILURE);
	}
	if(index >= list->len){
		fprintf(stderr, "[FATAL 0x%04X]: List index %zu out of bounds (len %zu).\n", 0x0405, index, list->len);
		exit(EXIT_FAILURE);
	}

	ListNode *curr = list->head;
	for(size_t i = 0; i < index; i++)
		curr = curr->next;

	curr->val = val;
}

uint64_t *listToArray(List *list){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot convert uninitialized list to array.\n", 0x0406);
		exit(EXIT_FAILURE);
	}

	uint64_t *arr = malloc(list->len * sizeof(uint64_t));
	if(!arr){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate array.\n", 0x0407);
		exit(EXIT_FAILURE);
	}

	ListNode *curr = list->head;
	for(size_t i = 0; i < list->len; i++){
		arr[i] = curr->val;
		curr = curr->next;
	}

	return arr;
}

// ==========
// Label List
// ==========

LabelList *labelListInit(void){
	LabelList *list = malloc(sizeof(LabelList));
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate LabelList.\n", 0x0410);
		exit(EXIT_FAILURE);
	}

	list->head = NULL;
	list->tail = NULL;
	list->len = 0;

	return list;
}

void labelListDestroy(LabelList *list){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot destroy uninitialized LabelList.\n", 0x0411);
		exit(EXIT_FAILURE);
	}

	LabelNode *curr = list->head;
	while(curr){
		LabelNode *next = curr->next;
		free(curr);
		curr = next;
	}

	free(list);
}

void labelListAppend(LabelList *list, const char *start, const char *end, size_t pc){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot append to uninitialized LabelList.\n", 0x0412);
		exit(EXIT_FAILURE);
	}

	LabelNode *node = malloc(sizeof(LabelNode));
	if(!node){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate LabelNode.\n", 0x0413);
		exit(EXIT_FAILURE);
	}

	node->start = start;
	node->end = end;
	node->pc = pc;
	node->next = NULL;

	if(!list->tail){
		list->head = node;
		list->tail = node;
	}
	else{
		list->tail->next = node;
		list->tail = node;
	}

	list->len++;
}

LabelNode *labelListFind(LabelList *list, const char *start, const char *end){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot search uninitialized LabelList.\n", 0x0414);
		exit(EXIT_FAILURE);
	}

	size_t qlen = (size_t)(end - start);
	LabelNode *curr = list->head;
	while(curr){
		if((size_t)(curr->end - curr->start) == qlen &&
		   memcmp(curr->start, start, qlen) == 0)
			return curr;
		curr = curr->next;
	}

	return NULL;
}

LabelNode *labelListGet(LabelList *list, size_t index){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot index into uninitialized LabelList.\n", 0x0415);
		exit(EXIT_FAILURE);
	}
	if(index >= list->len){
		fprintf(stderr, "[FATAL 0x%04X]: LabelList index %zu out of bounds (len %zu).\n", 0x0416, index, list->len);
		exit(EXIT_FAILURE);
	}

	LabelNode *curr = list->head;
	for(size_t i = 0; i < index; i++)
		curr = curr->next;

	return curr;
}

// ==========
// Line List
// ==========

LineList *lineListInit(void){
	LineList *list = malloc(sizeof(LineList));
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate LineList.\n", 0x0420);
		exit(EXIT_FAILURE);
	}

	list->head = NULL;
	list->tail = NULL;
	list->len = 0;
	list->totalBytes = 0;

	return list;
}

void lineListDestroy(LineList *list){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot destroy uninitialized LineList.\n", 0x0421);
		exit(EXIT_FAILURE);
	}

	LineNode *curr = list->head;
	while(curr){
		LineNode *next = curr->next;
		free(curr->line);
		free(curr);
		curr = next;
	}

	free(list);
}

void lineListAppend(LineList *list, const char *line){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot append to uninitialized LineList.\n", 0x0422);
		exit(EXIT_FAILURE);
	}

	LineNode *node = malloc(sizeof(LineNode));
	if(!node){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate LineNode.\n", 0x0423);
		exit(EXIT_FAILURE);
	}

	size_t len = strlen(line);
	node->line = malloc(len + 1);
	if(!node->line){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate line buffer.\n", 0x0424);
		exit(EXIT_FAILURE);
	}
	memcpy(node->line, line, len + 1);
	node->next = NULL;

	if(!list->tail){
		list->head = node;
		list->tail = node;
	}
	else{
		list->tail->next = node;
		list->tail = node;
	}

	list->len++;
	list->totalBytes += len;
}

char *lineListJoin(LineList *list, size_t *outLen){
	if(!list){
		fprintf(stderr, "[FATAL 0x%04X]: Cannot join uninitialized LineList.\n", 0x0425);
		exit(EXIT_FAILURE);
	}

	char *out = malloc(list->totalBytes + 1);
	if(!out){
		fprintf(stderr, "[FATAL 0x%04X]: Could not allocate join buffer.\n", 0x0426);
		exit(EXIT_FAILURE);
	}

	char *cursor = out;
	LineNode *curr = list->head;
	while(curr){
		size_t len = strlen(curr->line);
		memcpy(cursor, curr->line, len);
		cursor += len;
		curr = curr->next;
	}
	*cursor = '\0';

	if(outLen != NULL)
		*outLen = list->totalBytes;

	return out;
}

// 	.:
