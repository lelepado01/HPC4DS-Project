#pragma once

#include "global_constants.h"

#include "char_freq.h"
#include "message.h"
#include "huffman_tree.h"
#include "encoding.h"

#include "../utils/file_utils.h"
#include "../utils/time_utils.h"
#include "../utils/data_logger.h"
#include "../utils/conversion_utils.h"
#include "../utils/message_utils.h"

void recvEncodingText(EncodingText *encodingText, int sender);
void unorderedCharFreqDictSendRecv(int proc_number, int pid, CharFreqDictionary *dict, MPI_Datatype *charFreqDictType, bool withMaster);
void semiOrderedEncTextSendRecv(int pid, EncodingText *encodingText, int sender, int receiver);
