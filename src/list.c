#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

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
