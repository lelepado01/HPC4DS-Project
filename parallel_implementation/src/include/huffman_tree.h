#pragma once

#include "global_constants.h"

#include "char_freq.h"
#include "../utils/print_utils.h"

#include <stdlib.h>

typedef struct TreeNode {
	int frequency; 
	char character; 

	struct TreeNode *leftChild; 
	struct TreeNode *rightChild; 
} TreeNode; 

typedef struct LinkedListTreeNodeItem {
	TreeNode *item; 
	struct LinkedListTreeNodeItem *next; 
} LinkedListTreeNodeItem;

LinkedListTreeNodeItem* newNode(char character, int frequency);
LinkedListTreeNodeItem* getMinFreq(LinkedListTreeNodeItem *start);
LinkedListTreeNodeItem* orderedAppendToFreq(LinkedListTreeNodeItem *start, LinkedListTreeNodeItem *item);
LinkedListTreeNodeItem* createLinkedList(CharFreqDictionary *dict);

extern TreeNode* createHuffmanTree(CharFreqDictionary *dict);
extern void printHuffmanTree(TreeNode* root, int depth);
