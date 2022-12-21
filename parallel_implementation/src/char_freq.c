#include "include/char_freq.h"

// maybe we could improve the function by using the multythrading for loop
void getCharFreqsFromText(CharFreqDictionary* dict, char text[], long len, int pid) {
	for (int i = 0; i < len; i++) {
		char character = text[i]; 
		bool assigned = false; 
		for (int j = 0; j < dict->number_of_chars && !assigned; j++) {

            CharFreq *p = &dict->charFreqs[j];
			if (p->character == character) {
				++p->frequency; 
				assigned = true; 
			}
		}

		if (!assigned) {
            ++dict->number_of_chars;
            dict->charFreqs = realloc(dict->charFreqs, sizeof(CharFreq) * dict->number_of_chars);

            CharFreq *p = &dict->charFreqs[dict->number_of_chars - 1];
			*p = (struct CharFreq) {.character = character, .frequency = 1};
		}
	}
}

void sortCharFreqs(CharFreqDictionary* res) {
	int i, j;

	for (i = 0; i < res->number_of_chars; i++) {
		CharFreq minCharFreq = res->charFreqs[i];
		int indexOfMin = i;  
		for (j = i; j < res->number_of_chars; j++){
			if (minCharFreq.frequency > res->charFreqs[j].frequency) {
				minCharFreq = res->charFreqs[j]; 
				indexOfMin = j; 
			}
		}

		CharFreq temp = res->charFreqs[i]; 
		res->charFreqs[i] = minCharFreq;
		res->charFreqs[indexOfMin] = temp;
	}
}

void appendToCharFreqs(CharFreqDictionary* dict, char character, int pos) {
	if (pos == FIRST) {
		CharFreq *tmp = malloc(dict->number_of_chars * sizeof(CharFreq));
		memcpy(tmp, dict->charFreqs, dict->number_of_chars * sizeof(CharFreq));

		free(dict->charFreqs);
		int oldSize = dict->number_of_chars;
		++dict->number_of_chars;
		dict->charFreqs = malloc(dict->number_of_chars * sizeof(CharFreq));

		memcpy(dict->charFreqs + 1, tmp, oldSize * sizeof(CharFreq));

		dict->charFreqs[0].character = character;
		dict->charFreqs[0].frequency = 1;
	} else {
		int frequency = dict->charFreqs[dict->number_of_chars - 1].frequency + 1;
		dict->charFreqs = realloc(dict->charFreqs, sizeof(CharFreq) * (dict->number_of_chars+1));
		dict->charFreqs[dict->number_of_chars] = (struct CharFreq) {.character = character, .frequency = frequency};
		++dict->number_of_chars;
	}
}

void mergeCharFreqs(CharFreqDictionary* dict, CharFreq* charFreqs, int size) {
	for (int i = 0; i < size; i++) {
		char character = charFreqs[i].character;
		int frequency = charFreqs[i].frequency;
		bool assigned = false;
		for (int j = 0; j < dict->number_of_chars && !assigned; j++) {
			if (dict->charFreqs[j].character == character) {
				dict->charFreqs[j].frequency += frequency;
				assigned = true;
			}
		}

		if (!assigned)
			appendToCharFreqs(dict, character, LAST);
	}
}

// test if there is one faster than the other
// void mergeCharFreqs(CharFreqDictionary* dict, MsgDictionary* msg) {
// 	// int i, j;
// 	// for (i = 0; i < msg->charsNr; i++) {
// 	// 	bool found = false; 
// 	// 	for (j = 0; j < dict->number_of_chars && !found; j++) {
// 	// 		if (dict->charFreqs[j].character == msg->characters[i]) {
// 	// 			dict->charFreqs[j].frequency += msg->frequencies[i]; 
// 	// 			found = true; 
// 	// 		}
// 	// 	}

// 	// 	if (!found) {
// 	// 		++dict->number_of_chars; 
//     //         dict->charFreqs = realloc(dict->charFreqs, sizeof(CharFreq) * dict->number_of_chars);

//     //         CharFreq *p = &dict->charFreqs[dict->number_of_chars - 1];
// 	// 		p->character = msg->characters[i];
// 	// 		p->frequency = msg->frequencies[i];
// 	// 	}
// 	// }
// }

void printCharFreqs(CharFreqDictionary* dict) {
	printf("Dictionary: \n");
	for (int i = 0; i < dict->number_of_chars; i++) {
			printf("\t%d.\tcharacter: ", i); 
			printFormattedChar(dict->charFreqs[i].character);
			printf("\tfrequence: %d\n", dict->charFreqs[i].frequency); 
	}
}
