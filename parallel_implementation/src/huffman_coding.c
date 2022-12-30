#include "include/huffman_coding.h"

int main(int argc, char *argv[]) {
	MPI_Init(NULL, NULL);

	int proc_number;
	int pid; 

	MPI_Comm_size(MPI_COMM_WORLD, &proc_number);
	MPI_Comm_rank(MPI_COMM_WORLD, &pid);

	int thread_count = strtol(argv[1], NULL, MAX_DIGITS);
	if (thread_count <= 0) {
		fprintf(stderr, "Invalid number of threads: %d\n", thread_count);
		return 1;
	}

	omp_set_dynamic(0);
	omp_set_num_threads(thread_count);

	initDataLogger();
	setDataLoggerReferenceProcess(0);
	addLogColumn(pid, "N.Processes");
	addLogColumn(pid, "N.Characters");
	addLogColumn(pid, "Time");

	takeTime(pid);

	char *text = NULL;
	long processes_text_length = readFilePortionForProcess(SRC_FILE, &text, pid, proc_number);

	if (processes_text_length <= 0) {
		fprintf(stderr, "Process %d: Error while reading file %s\n", pid, SRC_FILE);
		return 1;
	}

	printf("Process %d: %ld characters read\n", pid, processes_text_length);
	addLogData(pid, intToString(proc_number));
	addLogData(pid, intToString(processes_text_length));

	CharFreqDictionary allChars = {.number_of_chars = 0, .charFreqs = NULL};
	getCharFreqsFromText(&allChars, text, processes_text_length, pid);

	CharEncodingDictionary encodingsDict = {.number_of_chars = allChars.number_of_chars, .charEncoding = NULL};
	LinkedListTreeNodeItem* root = NULL;
	EncodingText encodingText = {.nr_of_dim = 0, .nr_of_bytes = 0, .nr_of_bits = 0, .dimensions = NULL, .encodedText = NULL};

	// send the character frequencies to the master process
	if (pid != 0) {
		int bufferSize = 0;
		BYTE *buffer = getMessage(&allChars, MSG_DICTIONARY, &bufferSize);
		if (buffer != NULL && bufferSize > 0)
			MPI_Send(buffer, bufferSize, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
		else {
			fprintf(stderr, "Process %d: Error while sending %s message to the master process\n", pid, getMsgName(MSG_DICTIONARY));
			return 2;
		}

		freeBuffer(buffer);
	} else { // receive the character frequencies from the other processes
		for (int i = 1; i < proc_number; i++) {
			MPI_Status status;
			int bufferSize = 0;

			BYTE *buffer = prepareForReceive(&status, &bufferSize, i, 0);
			MPI_Recv(buffer, bufferSize, MPI_BYTE, i, 0, MPI_COMM_WORLD, &status);

			CharFreqDictionary rcvChars = {.number_of_chars = 0, .charFreqs = NULL};
			setMessage(&rcvChars, buffer);

			mergeCharFreqs(&allChars, rcvChars.charFreqs, rcvChars.number_of_chars, LAST_R);

			freeBuffer(rcvChars.charFreqs);
			freeBuffer(buffer);
		}

		sortCharFreqs(&allChars);

		// creates the huffman tree
		root = createHuffmanTree(&allChars);
		getEncodingFromTree(&encodingsDict, &allChars, root->item);

		// send the complete encoding table to each process
		int bufferSize = 0;
		BYTE *buffer = getMessage(&encodingsDict, MSG_ENCODING_DICTIONARY, &bufferSize);
		if (buffer != NULL && bufferSize > 0)
			for (int i = 1; i < proc_number; i++)
				MPI_Send(buffer, bufferSize, MPI_BYTE, i, 0, MPI_COMM_WORLD);
		else {
			fprintf(stderr, "Process %d: Error while sending %s message to the slave processes\n", pid, getMsgName(MSG_ENCODING_DICTIONARY));
			return 2;
		}

		freeBuffer(buffer);

		// encode the text for process 0
		encodeStringToByteArray(&encodingText, &encodingsDict, text, processes_text_length);
	}

	// each process receives the encoding dictionary 
	// and encodes its portion of the text 
	// then sends the encoded text to the master process
	if (pid != 0) {
		MPI_Status status;
		int bufferSize = 0;

		BYTE *buffer = prepareForReceive(&status, &bufferSize, 0, 0);
		MPI_Recv(buffer, bufferSize, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);

		setMessage(&encodingsDict, buffer);

		freeBuffer(buffer);

		encodeStringToByteArray(&encodingText, &encodingsDict, text, processes_text_length);

		// send to master process the encoded text
		bufferSize = 0;
		buffer = getMessage(&encodingText, MSG_ENCODING_TEXT, &bufferSize);

		if (buffer != NULL && bufferSize > 0)
			MPI_Send(buffer, bufferSize, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
		else {
			fprintf(stderr, "Process %d: Error while sending %s message to the master process\n", pid, getMsgName(MSG_ENCODING_TEXT));
			return 2;
		}

		freeBuffer(buffer);
	}

	// master process receives the encoded text from each process
	// and writes it to the file
	if (pid == 0) {
		// write an empty header to the file
		FileHeader fileHeader = {.byteStartOfDimensionArray = 0};
		BYTE *startPos = (BYTE*)&fileHeader;
		writeBufferToFile(ENCODED_FILE, startPos, sizeof(unsigned int) * FILE_HEADER_ELEMENTS, WRITE_B, 0);
		
		if (DEBUG(pid)) {
			printf("Header size: %lu\n", sizeof(unsigned int) * FILE_HEADER_ELEMENTS);
			printf("Encoded arrayPosStartPos: %d\n", fileHeader.byteStartOfDimensionArray);
		}

		// write the encoded tree to the file
		int byteSizeOfTree;
		BYTE* encodedTree = encodeTreeToByteArray(root->item, &byteSizeOfTree);
		writeBufferToFile(ENCODED_FILE, encodedTree, byteSizeOfTree, APPEND_B, 0);

		if (DEBUG(pid))
			printf("Encoded tree size: %d\n", getByteSizeOfTree(root->item));

		int nodes = countTreeNodes(root->item);

		if (DEBUG(pid)) {
			printf("Huffman tree nodes number: %d\n", nodes);
			printHuffmanTree(root->item, 0);
		}

		// receive the encoded text from each process
		// store in unique buffer
		for (int i = 1; i < proc_number; i++) {
			MPI_Status status;
			int bufferSize = 0;

			BYTE *buffer = prepareForReceive(&status, &bufferSize, i, 0);
			MPI_Recv(buffer, bufferSize, MPI_BYTE, i, 0, MPI_COMM_WORLD, &status);

			EncodingText temp;
			setMessage(&temp, buffer);

			// not sure if this leaves spaces between bytes... probably yes
			// but we may make it work with the block sizes
			mergeEncodedText(&encodingText, &temp);

			if (DEBUG(pid))
				printf("Number of bytes: %d\n", encodingText.nr_of_bytes);

			freeBuffer(temp.dimensions);
			freeBuffer(temp.encodedText);
			freeBuffer(buffer);
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
			
			// printEncodedText(encodingText.encodedText, encodingText.nr_of_bytes);
			// printf("\n");
		}

		fileHeader.byteStartOfDimensionArray = sizeof(FileHeader) + byteSizeOfTree + encodingText.nr_of_bytes;
		startPos = (BYTE*)&fileHeader;
		writeBufferToFile(ENCODED_FILE, startPos, sizeof(unsigned int) * FILE_HEADER_ELEMENTS, WRITE_B_AT, 0);

		if (DEBUG(pid)) {
			printf("Total number of blocks: %d\n", encodingText.nr_of_dim);
			printf("Encoded file size: %d\n", getFileSize(ENCODED_FILE));
			printf("Original file size: %d\n", getFileSize(SRC_FILE));
		}
	}

	takeTime(pid);
	printTime(pid, "Time elapsed");
	// saveTime(pid, TIME_LOG_FILE, "Time elapsed");

	float time = getTime(pid, "Time elapsed");
	addLogData(pid, floatToString(time));

	freeBuffer(encodingText.dimensions);
	freeBuffer(encodingText.encodedText);
	freeLinkedList(root);

	for (int i = 0; i < encodingsDict.number_of_chars; i++)
		freeBuffer(encodingsDict.charEncoding[i].encoding);

	freeBuffer(encodingsDict.charEncoding);

	freeBuffer(text);
	freeBuffer(allChars.charFreqs);

	MPI_Finalize();

	return 0;
}
