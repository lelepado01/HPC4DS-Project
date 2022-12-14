#include "include/huffman_coding.h"

void timeCheckPoint(int pid, char* label){
	takeTime(pid);
	printTime(pid, label);
	float time = getTime(pid, label);
	addLogData(pid, floatToString(time));
}

int main() {
	MPI_Init(NULL, NULL);

	int proc_number;
	int pid; 

	MPI_Comm_size(MPI_COMM_WORLD, &proc_number);
	MPI_Comm_rank(MPI_COMM_WORLD, &pid);

	initDataLogger(MASTER_PROCESS, (pid == MASTER_PROCESS) ? true : false);
	addLogColumn(pid, "Read File");
	addLogColumn(pid, "Get Char Frequencies");
	addLogColumn(pid, "Sort Char Frequencies");
	addLogColumn(pid, "Create Huffman Tree");
	addLogColumn(pid, "Get Encoding from Tree");
	addLogColumn(pid, "Encode Single Text");
	addLogColumn(pid, "Write Encoded Text");
	takeTime(pid);

	CharFreqDictionary allChars = {.number_of_chars = 0, .charFreqs = NULL};
	LinkedListTreeNodeItem *root = NULL;
	CharEncodingDictionary encodingDict = {.number_of_chars = 0, .charEncoding = NULL};
	EncodingText encodingText = {.nr_of_dim = 0, .nr_of_bytes = 0, .nr_of_bits = 0, .dimensions = NULL, .encodedText = NULL};

	char *text = NULL;
	long processes_text_length = readFilePortionForProcess(SRC_FILE, &text, pid, proc_number);

	if (processes_text_length <= 0) {
		fprintf(stderr, "Process %d: Error while reading file %s\n", pid, SRC_FILE);
		return 1;
	}

	// printf("Process %d: %ld characters read\n", pid, processes_text_length);
	addLogData(pid, intToString(proc_number));
	addLogData(pid, intToString(0));
	addLogData(pid, intToString(processes_text_length));

	timeCheckPoint(pid, "Read File");

	getCharFreqsFromText(&allChars, text, processes_text_length, pid);

	timeCheckPoint(pid, "Get Char Frequencies");

	oddEvenSort(&allChars);

	timeCheckPoint(pid, "Sort Char Frequencies");

	// takeTime(pid);
	// printTime(pid, "Time to sort frequencies");

	// creates the huffman tree
	root = createHuffmanTree(&allChars);

	timeCheckPoint(pid, "Create Huffman Tree");

	// takeTime(pid);
	// printTime(pid, "Time to create tree");

	// creates the encoding dictionary
	getEncodingFromTree(&encodingDict, &allChars, root->item);

	timeCheckPoint(pid, "Get Encoding from Tree");

	// takeTime(pid);
	// printTime(pid, "Time to get encoding dictionary");

	encodeStringToByteArray(&encodingText, &encodingDict, text, processes_text_length);

	timeCheckPoint(pid, "Encode Single Text");

	// takeTime(pid);
	// printTime(pid, "Time to encode text");

	// convert tree into a suitable form for writing to file
	int byteSizeOfTree;
	BYTE* encodedTree = encodeTreeToByteArray(root->item, &byteSizeOfTree);
	int nodes = countTreeNodes(root->item);

	// write the header
	FileHeader fileHeader = {.byteStartOfDimensionArray = sizeof(FileHeader) + byteSizeOfTree + encodingText.nr_of_bytes};
	BYTE *startPos = (BYTE*)&fileHeader;
	writeBufferToFile(ENCODED_FILE, startPos, sizeof(unsigned int) * FILE_HEADER_ELEMENTS, WRITE_B, 0);

	if (DEBUG(pid)) {
		printf("Header size: %lu\n", sizeof(unsigned int) * FILE_HEADER_ELEMENTS);
		printf("Encoded arrayPosStartPos: %d\n", fileHeader.byteStartOfDimensionArray);
	}

	// write the huffman tree
	writeBufferToFile(ENCODED_FILE, encodedTree, byteSizeOfTree, APPEND_B, 0);

	if (DEBUG(pid)) {
		printf("Encoded tree size: %d\n", getByteSizeOfTree(root->item));
		printf("Huffman tree nodes number: %d\n", nodes);
		printHuffmanTree(root->item, 0);
	}

	writeBufferToFile(ENCODED_FILE, encodingText.encodedText, encodingText.nr_of_bytes, APPEND_B, 0);

	if (DEBUG(pid))
		printf("Encoded text size: %d\n", encodingText.nr_of_bytes);

	BYTE *dimensions = (BYTE*)encodingText.dimensions;
	writeBufferToFile(ENCODED_FILE, dimensions, encodingText.nr_of_dim * sizeof(unsigned short), APPEND_B, 0);

	if (DEBUG(pid)) {
		printf("Dimensions array size: %ld\n", encodingText.nr_of_dim * sizeof(unsigned short));
		for (int i = 0; i < encodingText.nr_of_dim; i++)
			printf("\tdimension[%d] = %d\n", i, encodingText.dimensions[i]);
	}

	fileHeader.byteStartOfDimensionArray = sizeof(FileHeader) + byteSizeOfTree + encodingText.nr_of_bytes;
	startPos = (BYTE*)&fileHeader;
	writeBufferToFile(ENCODED_FILE, startPos, sizeof(unsigned int) * FILE_HEADER_ELEMENTS, WRITE_B_AT, 0);

	if(DEBUG(pid)) {
		printf("Total number of blocks: %d\n", encodingText.nr_of_dim);
		printf("Encoded file size: %d\n", getFileSize(ENCODED_FILE));
		printf("Original file size: %d\n", getFileSize(SRC_FILE));
	}

	// saveTime(pid, TIME_LOG_FILE, "Time elapsed");

	// float time = getTime(pid, "Time elapsed");
	// addLogData(pid, floatToString(time));

	timeCheckPoint(pid, "Write Encoded Text");

	terminateDataLogger();

	freeLinkedList(root);

	for (int i = 0; i < encodingDict.number_of_chars; i++)
		freeBuffer(encodingDict.charEncoding[i].encoding);

	freeBuffer(encodingDict.charEncoding);

	freeBuffer(encodingText.dimensions);
	freeBuffer(encodingText.encodedText);

	freeBuffer(text);
	freeBuffer(allChars.charFreqs);

	MPI_Finalize();

	return 0;
}
