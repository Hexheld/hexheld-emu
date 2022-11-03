#ifndef __CPU_DECODE_H__
#define __CPU_DECODE_H__

#include "types.h"
#include "pilot.h"

typedef struct {
	Pilot_system *sys;
	
	inst_decoded_flags work_regs;
	uint16_t pc;
	uint8_t k;
	uint8_t inst_length;
	uint8_t words_to_read;
	enum
	{
		DECODER_HALF1_DISPATCH_WAIT,
		DECODER_HALF1_READY,
		DECODER_HALF1_READ_INST_WORD,
		
		DECODER_HALF2_READ_OPERANDS,
		DECODER_HALF2_DISPATCH
	} decoding_phase;
} pilot_decode_state;

void decode_unreachable_ ();

// Queues in a word read from the fetch unit
void decode_queue_read_word_ (pilot_decode_state *state);

// Tries to actually read a word from the fetch unit
bool decode_try_read_word_ (pilot_decode_state *state);

#endif
