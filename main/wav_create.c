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


int alloc_wav_data( int16_t** data, int seconds_of_recording, int num_channels, int bits_per_sample, int sample_rate ) {

	int samples = seconds_of_recording * num_channels * sample_rate;
	int len = samples * bits_per_sample / 8;

	printf( "Length: %d bytes Samples: %d icnt: %d\n", len, samples, seconds_of_recording * sample_rate );

	*data = (int16_t *) malloc(len + sizeof(wav_header_t));

	if ( *data == NULL )
		printf( "Could not alloc %d bytes\n", len + sizeof(wav_header_t) );

	return len;
}


void create_wav_data( int16_t* buf, int seconds_of_recording, double frequency, int num_channels, int bits_per_sample, int sample_rate )
{
	float total_count = seconds_of_recording * sample_rate;

    for( int i = 0 ; i < total_count ; i++ )
    {

//    	double sin_float = 15000 * sinf( 2 * i * M_PI / ( sample_rate / frequency ) );


    	double sin_float = 25000 * (
        		sinf( 2 * i * M_PI / ( sample_rate / frequency ) ) +
				sinf( 2 * i * M_PI / ( sample_rate / ( 3 * frequency ) ) ) / 3 +
        		sinf( 2 * i * M_PI / ( sample_rate / ( 5 * frequency ) ) ) / 5 +
        		sinf( 2 * i * M_PI / ( sample_rate / ( 7 * frequency ) ) ) / 7 +
        		sinf( 2 * i * M_PI / ( sample_rate / ( 9 * frequency ) ) ) / 9 +
        		sinf( 2 * i * M_PI / ( sample_rate / ( 11 * frequency ) ) ) / 11 +
        		sinf( 2 * i * M_PI / ( sample_rate / ( 13 * frequency ) ) ) / 13
				);


        int16_t lval = sin_float;
        int16_t rval = sin_float;

        buf[i*2] = lval;
        buf[i*2+1] = rval;

    }

}


void create_wav_header( int16_t* data_buf, int len, int num_channels, int bits_per_sample, int sample_rate )
{
	wav_header_t* w = (wav_header_t*) data_buf;

	w->riff.chunk_id = 0X46464952;			// "RIFF"
	w->riff.format = 0X45564157;			// "WAVE"
	w->riff.chunk_size = len + 36;

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

int create_wav( int seconds_of_recording, int frequency, int16_t** data ) {

	int num_channels = 2;
	int bits_per_sample = 16;
	int sample_rate = 16000;

	int len = alloc_wav_data( data, seconds_of_recording, num_channels, bits_per_sample, sample_rate );
	int16_t* data_buf = *data;

	create_wav_header( data_buf, len, num_channels, bits_per_sample, sample_rate );
	create_wav_data( data_buf + sizeof(wav_header_t)/2, seconds_of_recording, frequency, num_channels, bits_per_sample, sample_rate );

	return len + sizeof(wav_header_t);
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
