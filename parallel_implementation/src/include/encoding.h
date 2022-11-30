
#pragma once

#include "global_constants.h"
#include "char_freq.h"
#include "huffman_tree.h"

#include "../utils/print_utils.h"

typedef struct CharEncoding {
	char character; 
	char* encoding; 
} CharEncoding; 

CharEncoding* getEncodingFromTree(CharFreqDictionary* allLetters, TreeNode* root); 

extern bool findEncodingFromTree(char letter, TreeNode* root, char* dst, int depth);
extern void printEncodings(CharEncoding* encodings, int size);