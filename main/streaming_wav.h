/*
 * streaming_wav.h
 *
 *  Created on: Jun 16, 2021
 *      Author: xenir
 */

#ifndef MAIN_STREAMING_WAV_H_
#define MAIN_STREAMING_WAV_H_

#include <stdint.h>
#include "wav_header.h"


typedef struct {

	wav_header_t	hdr;
	int16_t			*buf;
	int				buf_size;
	int				cnt;

} streaming_wav_t;

void streaming_wav_init( streaming_wav_t* wav, int buffer_size );
void streaming_wav_play( streaming_wav_t* wav );
void streaming_wav_destroy( streaming_wav_t* wav );

#endif /* MAIN_STREAMING_WAV_H_ */
