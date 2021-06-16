/*
 * wav_header.h
 *
 *  Created on: Jun 15, 2021
 *      Author: xenir
 */

#ifndef WAV_HEADER_H_
#define WAV_HEADER_H_

typedef struct {
    uint32_t chunk_id;               /*!<chunk id;"RIFF",0X46464952 */
    uint32_t chunk_size;             /*!<file length - 8 */
    uint32_t format;                /*!<WAVE,0X45564157 */
} __attribute__((packed)) chunk_riff_t;

typedef struct {
    uint32_t chunk_id;               /*!<chunk id;"fmt ",0X20746D66 */
    uint32_t chunk_size;             /*!<Size of this fmt block (Not include ID and Size);16 or 18 or 40 bytes. */
    uint16_t audio_format;           /*!<format;0X01:linear PCM;0X11:IMA ADPCM */
    uint16_t num_of_channels;         /*!<Number of channel;1: 1 channel;2: 2 channels; */
    uint32_t samplerate;            /*!<sample rate;0X1F40 = 8Khz */
    uint32_t byterate;              /*!<Byte rate; */
    uint16_t block_align;            /*!<align with byte; */
    uint16_t bits_per_sample;         /*!<Bit lenght per sample point,4 ADPCM */
} __attribute__((packed)) chunk_fmt_t;

typedef struct {
    uint32_t chunk_id;                  /*!<chunk id;"data",0X5453494C */
    uint32_t chunk_size;                /*!<Size of data block(Not include ID and Size) */
} __attribute__((packed)) chunk_data_t;


typedef struct {
    chunk_riff_t riff;                    /*!<riff */
    chunk_fmt_t fmt;                      /*!<fmt */
    chunk_data_t data;                    /*!<data */
} __attribute__((packed)) wav_header_t;


/*

The canonical WAVE format starts with the RIFF header:

0         4   ChunkID          Contains the letters "RIFF" in ASCII form
                               (0x52494646 big-endian form).
4         4   ChunkSize        36 + SubChunk2Size, or more precisely:
                               4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
                               This is the size of the rest of the chunk
                               following this number.  This is the size of the
                               entire file in bytes minus 8 bytes for the
                               two fields not included in this count:
                               ChunkID and ChunkSize.
8         4   Format           Contains the letters "WAVE"
                               (0x57415645 big-endian form).

The "WAVE" format consists of two subchunks: "fmt " and "data":
The "fmt " subchunk describes the sound data's format:

12        4   Subchunk1ID      Contains the letters "fmt "
                               (0x666d7420 big-endian form).
16        4   Subchunk1Size    16 for PCM.  This is the size of the
                               rest of the Subchunk which follows this number.
20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
                               Values other than 1 indicate some
                               form of compression.
22        2   NumChannels      Mono = 1, Stereo = 2, etc.
24        4   SampleRate       8000, 44100, etc.
28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
32        2   BlockAlign       == NumChannels * BitsPerSample/8
                               The number of bytes for one sample including
                               all channels. I wonder what happens when
                               this number isn't an integer?
34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
          2   ExtraParamSize   if PCM, then doesn't exist
          X   ExtraParams      space for extra parameters

The "data" subchunk contains the size of the data and the actual sound:

36        4   Subchunk2ID      Contains the letters "data"
                               (0x64617461 big-endian form).
40        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
                               This is the number of bytes in the data.
                               You can also think of this as the size
                               of the read of the subchunk following this
                               number.
44        *   Data             The actual sound data.

*/


#endif /* WAV_HEADER_H_ */
