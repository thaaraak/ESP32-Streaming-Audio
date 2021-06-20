/*
 * wav_create.c
 *
 *  Created on: Jun 15, 2021
 *      Author: xenir
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "wav_header.h"
#include "streaming_wav.h"

void streaming_wav_play( streaming_wav_t* wav, float frequency ) {

    for( int j = 0 ; j < wav->buf_size ; j++ )
    {

    	int i = wav->cnt++;
    	if ( wav->cnt > 64000 )
    		wav->cnt = 0;

    	int sample_rate = wav->hdr.fmt.samplerate;

    	/*
    	if ( i % 16000 == 0 )
    		printf( "Cnt: %d Frequency: %f\n", i, frequency );
		*/

    	double sin_float = 15000 * sinf( 2 * i * M_PI / ( sample_rate / frequency ) );

/*
    	double sin_float = 15000 * (
        		sinf( 2 * i * M_PI / ( sample_rate / frequency ) ) +
				sinf( 2 * i * M_PI / ( sample_rate / ( 3 * frequency ) ) ) / 3 +
        		sinf( 2 * i * M_PI / ( sample_rate / ( 5 * frequency ) ) ) / 5
				);
*/

        int16_t lval = sin_float;
        int16_t rval = sin_float;

        if ( wav->hdr.fmt.num_of_channels == 2 ) {
        	wav->buf[j*2] = lval;
        	wav->buf[j*2+1] = rval;
        } else {
        	wav->buf[j] = lval;
        }

    }

}


void streaming_wav_header( streaming_wav_t* wav, int num_channels, int bits_per_sample, int sample_rate )
{
	wav_header_t* w = (wav_header_t*) &(wav->hdr);

	int len = 0xFFFFFFFF;

	w->riff.chunk_id = 0X46464952;			// "RIFF"
	w->riff.format = 0X45564157;			// "WAVE"
//	w->riff.chunk_size = len + 36;
	w->riff.chunk_size = len;

	w->fmt.chunk_id = 0X20746D66;			// "fmt "
	w->fmt.audio_format = 1;
	w->fmt.bits_per_sample = bits_per_sample;
	w->fmt.block_align = num_channels * bits_per_sample/8;
	w->fmt.byterate = sample_rate * num_channels * bits_per_sample/8;
	w->fmt.chunk_size = 16;
	w->fmt.num_of_channels = num_channels;
	w->fmt.samplerate = sample_rate;

	w->data.chunk_id = 0X61746164;
	w->data.chunk_size = len;
}


int streaming_wav_factor( streaming_wav_t* wav ) {

	return (
			wav->hdr.fmt.num_of_channels *
			wav->hdr.fmt.bits_per_sample / 8 );
}

void streaming_wav_destroy( streaming_wav_t* wav ) {

	free(wav->buf);
}


void streaming_wav_init( streaming_wav_t* wav, int buffer_size ) {

	int num_channels = 1;
	int bits_per_sample = 16;
	int sample_rate = 16000;

	wav->cnt = 0;

	streaming_wav_header( wav, num_channels, bits_per_sample, sample_rate );

	wav->buf = (int16_t*)malloc( buffer_size );
	wav->buf_size = buffer_size / streaming_wav_factor( wav );

}



/*
int main(void) {

	int16_t* data;

	int total = create_wav( 2, 1000, &data );

	int fd;

	if (( fd = open( "sample.wav", O_RDWR | O_CREAT | O_TRUNC, 0777 ) ) < 0 ) {
		printf( "Error opening file\n");
		return -1;
	}

	char* buf = (char*) data;

	printf( "Total: %d bytes\n", total );

	while ( total > 0 ) {
		ssize_t wrote = write( fd, buf, total );
		printf( "Wrote: %ld bytes\n", wrote );
		total -= wrote;
	}

	close(fd);
	free( data );


}
*/
