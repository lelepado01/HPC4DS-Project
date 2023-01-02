#include "include/huffman_decoding.h"

int roundUp(int numToRound, int multiple) {
    if (multiple == 0)
        return numToRound;

    int remainder = abs(numToRound) % multiple;
    if (remainder == 0)
        return numToRound;

    if (numToRound < 0)
        return -(abs(numToRound) - remainder);
    else
        return numToRound + multiple - remainder;
}

int calculatePrevTextSize(unsigned short *dimensions, int nrOfBlocks) {
	int prevTextSize = 0;

	#pragma omp parallel for num_threads(omp_get_max_threads()) reduction(+:prevTextSize)
	for (int i = 0; i < nrOfBlocks; i++) {
		// size of block in bits
		int blockSize = dimensions[i]; 

		// round up to multiple of 8
		if (blockSize % BITS_IN_BYTE != 0)
			blockSize = roundUp(blockSize, BITS_IN_BYTE);

		// convert to bytes 
		blockSize /= BITS_IN_BYTE;
		prevTextSize += blockSize;
	}

	return prevTextSize;
}

void calculateBlockRange(int nrOfBlocks, int nrOfProcs, int pid, int *start, int *end) {
    int quotient = nrOfBlocks / nrOfProcs;
    int quoto =	nrOfBlocks % nrOfProcs;

	*start = (pid * quotient);
	if (quoto != 0 && pid > 0)
		*start += (pid > quoto) ? quoto : pid;

	*end = *start + quotient;
	if (quoto != 0 && (pid == 0 || pid < quoto))
		++(*end);
}

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

	FileHeader header = {.byteStartOfDimensionArray = 0};
	TreeNode *root = calloc(1, sizeof(TreeNode));
	DecodingText decodingText = {.length = 0, .decodedText = NULL};

	takeTime(pid);
	
	FILE *fp = openFile(ENCODED_FILE, READ_B, 0);
	if (fp == NULL) {
		fprintf(stderr, "Process %d: Error opening file %s\n", pid, ENCODED_FILE);
		return 1;
	}

	parseHeader(&header, fp);

	if (DEBUG(pid)) {
		printf("Header size: %lu\n", FILE_HEADER_ELEMENTS * sizeof(unsigned int));
		printf("Encoded arrayPosStartPos: %d\n", header.byteStartOfDimensionArray);
	}

	parseHuffmanTree(root, fp);
	int nodes = countTreeNodes(root);
	int treeByteSize = nodes * sizeof(TreeArrayItem);

	if (DEBUG(pid)) {
		printf("Encoded tree size: %d\n", treeByteSize);
		printf("Huffman tree nodes number: %d\n", nodes);
		printHuffmanTree(root, 0);
	}

	int fileSize = getFileSize(ENCODED_FILE);
	int number_of_blocks = (fileSize - header.byteStartOfDimensionArray) / sizeof(unsigned short);
	unsigned short *dimensions = calloc(number_of_blocks, sizeof(unsigned short));
	parseBlockLengths(dimensions, fp, number_of_blocks, header.byteStartOfDimensionArray);

	if (DEBUG(pid))
		for (int i = 0; i < number_of_blocks; i++)
			printf("dimensions[%d]: %d\n", i, dimensions[i]);

	int start = 0;
	int end = 0;
	calculateBlockRange(number_of_blocks, proc_number, pid, &start, &end);
	
	if (DEBUG(pid))
		printf("Process %d - Block range: %d - %d - Blocks nr: %d\n", pid, start, end - 1, end - start);

	int startPos = (sizeof(FileHeader) * FILE_HEADER_ELEMENTS) + treeByteSize;
	startPos += (pid != 0) ? calculatePrevTextSize(dimensions, start) : 0;

	char *decodedText = decodeFromFile(
		startPos,
		dimensions,
		start,
		end - start,
		fp,
		root);

	fclose(fp);

	if (pid != 0) {
		MsgHeader header = {.id = MSG_TEXT, .size = 0, .type = NULL, .position = 0};
		BYTE *buffer = getMessage(&header, decodedText);

		if (buffer == NULL || header.size <= 0) {
			fprintf(stderr, "Process %d: Error while creating message %s\n", pid, getMsgName(header.id));
			return 1;
		}

		MPI_Send(buffer, header.position, MPI_PACKED, 0, 0, MPI_COMM_WORLD);

		freeBuffer(buffer);
	} else {
		decodingText.length = strlen(decodedText) + 1;
		decodingText.decodedText = decodedText;

		for (int i = 1; i < proc_number; i++) {
			MPI_Status status;
			DecodingText rcvText = {.length = 0, .decodedText = NULL};
			MsgProbe probe = {.header.id = MSG_TEXT, .header.size = 0, .header.type = NULL, .header.position = 0, .pid = i, .tag = 0};

			BYTE *buffer = prepareForReceive(&probe, &status);

			MPI_Recv(buffer, probe.header.size, MPI_PACKED, probe.pid, probe.tag, MPI_COMM_WORLD, &status);
			setMessage(&probe.header, &rcvText, buffer);

			mergeDecodedText(&decodingText, &rcvText);

			freeBuffer(rcvText.decodedText);
			freeBuffer(buffer);
		}

		printf("\nDecoded text:\n%s\n", decodingText.decodedText);
	}

	takeTime(pid);
	printTime(pid, "Time elapsed");
	// saveTime(pid, LOG_FILE, "Time elapsed");

	freeBuffer(decodingText.decodedText);
	freeBuffer(dimensions);
	freeTree(root);

	MPI_Finalize();

	return 0;
}