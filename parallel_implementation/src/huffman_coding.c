#include "include/huffman_coding.h"

// int find_encoding(char letter, struct TreeNode* root, char* dst, int depth){

// 	int found = 0;
// 	if (root->letter == letter){
// 		dst[depth] = '\0';
// 		return 1;
// 	} else {

// 		if (root->leftChild != NULL){
// 			dst[depth] = '0';
// 			found = find_encoding(letter, root->leftChild, dst, depth+1);
// 		}

// 		if (found == 0 && root->rightChild != NULL){
// 			dst[depth] = '1';
// 			found = find_encoding(letter, root->rightChild, dst, depth+1);
// 		}
// 	}

// 	return found;
// }

// void get_encoding_from_tree(struct LetterFreqDictionary* allLetters, struct TreeNode* root, struct LetterEncoding* encodings){

// 	for (int i = 0; i < allLetters->number_of_letters; i++){

// 		encodings[i].letter = allLetters->letterFreqs[i].letter;
// 		encodings[i].encoding = malloc(sizeof(char) * allLetters->number_of_letters);

// 		find_encoding(allLetters->letterFreqs[i].letter, root, encodings[i].encoding, 0);
// 	}
// }

// void encode_to_file(char* text, struct LetterEncoding* encodings, int unique_letters, int total_letters){

// 	FILE *fp;
// 	fp = fopen(ENCODED_FILE, "wb");

// 	int charIndex = 0;
// 	char c = 0;
// 	for (int i = 0; i < total_letters; i++) {
// 		for (int j = 0; j < unique_letters; j++) {
// 			if (text[i] == encodings[j].letter) {
// 				append_string_to_binary_file(encodings[j].encoding, fp, &charIndex, &c);
// 			}
// 		}
// 	}

// 	// appends and writes custom end of file character
// 	append_string_to_binary_file(encodings[unique_letters-1].encoding, fp, &charIndex, &c);
// 	fwrite(&c, sizeof(char), 1, fp);

// 	fclose(fp);
// }

// void decode_from_file(struct TreeNode* root){

// 	FILE *fp2;
// 	fp2 = fopen(ENCODED_FILE, "rb");

// 	char c;
// 	char lastContinuousChar = 0;

// 	struct TreeNode* intermediateNode = root;

// 	int endReached = 0;
// 	while (fread(&c, sizeof(char), 1, fp2)) {

// 		if (endReached == 1) {
// 			break;
// 		}

// 		for (int i = 0; i < 8; i++) {
// 			if (intermediateNode->letter != '$') {

// 				if (intermediateNode->letter == '#'){
// 					endReached = 1;
// 					break;
// 				}

// 				if (VERBOSE){
// 					printf("%c", intermediateNode->letter);
// 				}

// 				intermediateNode = root;
// 			}

// 			if (c & (1 << i)) {
// 				intermediateNode = intermediateNode->rightChild;
// 			} else {
// 				intermediateNode = intermediateNode->leftChild;
// 			}
// 		}
// 	}

// 	fclose(fp2);
// }

void freeBuffer(void* buffer) {
	if (buffer != NULL) {
		free(buffer);
	}
}

int main()
{
	MPI_Init(NULL, NULL);

	int proc_number;
	int pid; 

	MPI_Comm_size(MPI_COMM_WORLD, &proc_number);
	MPI_Comm_rank(MPI_COMM_WORLD, &pid);

	int i;

	// get the processes' portion of text
	char *text = NULL;
	long total_text_length = read_file(SRC_FILE, &text, pid, proc_number);

	// get characters frequencies for the processes' portion of text
	CharFreqDictionary allChars = {.number_of_chars = 0, .charFreqs = NULL};
	get_chars_freqs(&allChars, text, total_text_length, pid);

	MPI_Datatype dictType;
	MsgDictionary msgDictSnd;
	MsgDictionary msgDictRcv;

	initMsgDictionary(&msgDictSnd.header);
	setMsg(&allChars, (MsgGeneric*)&msgDictSnd, &dictType);

	// print the msg dictionary
	// printf("Process %d: %d\t%d\t%c\t%d\n", pid, msgDictSnd->header.id, msgDictSnd->header.size, msgDictSnd->characters[msgDictSnd->charsNr-1], msgDictSnd->frequencies[msgDictSnd->charsNr-1]);

	if (pid != 0) {
		// send CharFreqDictionary to master process

		// maybe we could use the send version that uses the mpi buffer 
		// in this way we can empty the msgDict without risks 
		printf("Process %d is sending %d characters to master process\n", pid, msgDictSnd.charsNr);
		MPI_Send(&msgDictSnd, 1, dictType, 0, 0, MPI_COMM_WORLD);
		printf("Process %d sent %d characters to master process\n", pid, msgDictSnd.charsNr);

	} else {
		// master process receives all the slaves processes
		printf("num of processes: %d", proc_number);
	 	for (i = 1; i < proc_number; i++) {
			printf("\nProcess dest %d - process send %d: i'm here 0\n", pid, i);

			MPI_Status status;

			printf("\nProcess dest %d - process send %d: i'm here 1\n", pid, i);

			// Now receive the message with the allocated buffer
			MPI_Recv(&msgDictRcv, 1, dictType, i, 0, MPI_COMM_WORLD, &status);

			printf("\nProcess dest %d - process send %d: i'm here 2\n", pid, i);

			getMsg(&allChars, (MsgGeneric*) &msgDictRcv);

			printf("Process %d: %d, passed\n", pid, allChars.number_of_chars);

			freeBuffer(msgDictRcv.characters);
			freeBuffer(msgDictRcv.frequencies);

			freeBuffer(msgDictSnd.characters);
			freeBuffer(msgDictSnd.frequencies);
			
			printf("////////////////////////////////////////////////////\n\n");
		}
	}

	// not the most elegant solution for distinguish 
	// between the processes, but it works
	// we will modify it later
	if (pid == 0) {
		#ifdef VERBOSE <= 3
			printf("Before sorting:\n");
			print_dictionary(&allChars, pid);
		#endif

		// sort the LetterFreqDictionary only in the master process
		sort_freqs(&allChars);

		#ifdef VERBOSE <= 3
			printf("After sorting:\n");
			print_dictionary(&allChars, pid);
		#endif

		// append the sync character to the LetterFreqDictionary
		append_to_freq(&allChars, '\0', FIRST);

		#ifdef VERBOSE <= 3
			printf("After appending:\n");
			print_dictionary(&allChars, pid);
		#endif

		printf("\nProcess %d: i'm here 45\n", pid);
	}

	freeBuffer(msgDictSnd.characters);
	freeBuffer(msgDictSnd.frequencies);

	MPI_Type_free(&dictType);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// 	// create the Huffman tree
	// 	struct TreeNode* root = create_huffman_tree(allLetters);

	// 	// get the encodings for each letter
	// 	struct LetterEncoding* encodings = malloc(sizeof(struct LetterEncoding) * allLetters->number_of_letters);
	// 	get_encoding_from_tree(allLetters, root, encodings);

	// 	// send encoding table to all processes
	// 	for (int i = 1; i < NUM_OF_PROCESSES; i++) {
	// 		MPI_Send(encodings, sizeof(struct LetterEncoding) * allLetters->number_of_letters, MPI_BYTE, i, 0, MPI_COMM_WORLD);
	// 	}
	// }

	// // all processes receive the encoding table
	// if (pid != 0) {
	// 	struct LetterEncoding* encodings = malloc(sizeof(struct LetterEncoding) * allLetters->number_of_letters);
	// 	MPI_Recv(encodings, sizeof(struct LetterEncoding) * allLetters->number_of_letters, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	// 	// noi abbiamo fatto solo encode to file, ma servirebbe un encode to byte array,
	// 	// che poi viene mandato al master process, che lo scrive su file (questa riga di commento è stata scritta da copilot)

	// 	// encode the processes' portion of text to buffer
	// 	char* buffer = encode_to_buffer(subtext, encodings, allLetters->number_of_letters);

	// 	// add to buffer sync character
	// 	buffer = strcat(buffer, encodings[allLetters->number_of_letters-1].encoding); //TODO: check if this is correct

	// 	// send the buffer to the master process
	// 	MPI_Send(buffer, strlen(buffer), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
	// }

	// // master process receives all buffers
	// if (pid == 0) {
	// 	FILE *fp;
	// 	fp = fopen(ENCODED_FILE, "wb");

	// 	// master writes his own buffer to file
	// 	fwrite(buffer, sizeof(char), strlen(buffer), fp);

	// 	for (int i = 1; i < NUM_OF_PROCESSES; i++) {
	// 		// receive buffer
	// 		char* buffer = malloc(sizeof(char) * total_text_length);
	// 		MPI_Recv(buffer, total_text_length, MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	// 		// write buffer to file
	// 		fwrite(buffer, sizeof(char), strlen(buffer), fp);
	// 	}

	// 	fclose(fp);
	// }

	// DECODING
	// decode_from_file(root);

	// if (VERBOSE){
	// 	struct timespec end = get_time();
	// 	printf("\nDecoding time: %f seconds\n", get_execution_time(start, end));
	// }

	// printf("\nCompression stats: \n");
	// printf("Original file size: %d bits\n", get_file_size("text.txt"));
	// printf("Compressed file size: %d bits\n", get_file_size("output"));
	// printf("Compression rate: %.2f%%\n", (1 - (double)get_file_size("output") / (double)get_file_size("text.txt")) * 100);

	// printf("\n");

	freeBuffer(allChars.charFreqs);
	freeBuffer(text);

	MPI_Finalize();

	return 0;
}