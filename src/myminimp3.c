/*#define MINIMP3_ONLY_MP3*/
/*#define MINIMP3_ONLY_SIMD*/
/*#define MINIMP3_NONSTANDARD_BUT_LOGICAL*/
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ALLOW_MONO_STEREO_TRANSITION
#include "minimp3_ex.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "myminimp3.h"

static int16_t read16le(const void *p)
{
    const uint8_t *src = (const uint8_t *)p;
    return ((src[0]) << 0) | ((src[1]) << 8);
}

static char *wav_header(int hz, int ch, int bips, int data_bytes)
{
    static char hdr[44] = "RIFFsizeWAVEfmt \x10\0\0\0\1\0ch_hz_abpsbabsdatasize";
    unsigned long nAvgBytesPerSec = bips*ch*hz >> 3;
    unsigned int nBlockAlign      = bips*ch >> 3;

    *(int32_t *)(void*)(hdr + 0x04) = 44 + data_bytes - 8;   /* File size - 8 */
    *(int16_t *)(void*)(hdr + 0x14) = 1;                     /* Integer PCM format */
    *(int16_t *)(void*)(hdr + 0x16) = ch;
    *(int32_t *)(void*)(hdr + 0x18) = hz;
    *(int32_t *)(void*)(hdr + 0x1C) = nAvgBytesPerSec;
    *(int16_t *)(void*)(hdr + 0x20) = nBlockAlign;
    *(int16_t *)(void*)(hdr + 0x22) = bips;
    *(int32_t *)(void*)(hdr + 0x28) = data_bytes;
    return hdr;
}

static void decode_file(const char *input_file_name, const unsigned char *buf_ref, int ref_size, FILE *file_out, const int wave_out)
{
    mp3dec_t mp3d;
    int i, data_bytes, total_samples = 0, maxdiff = 0;
    double MSE = 0.0, psnr;

    mp3dec_file_info_t info;

    if (mp3dec_load(&mp3d, input_file_name, &info, 0, 0))
    {
        printf("error: file not found or read error");
        exit(1);
    }

    int16_t *buffer = info.buffer;

    if (wave_out && file_out)
        fwrite(wav_header(0, 0, 0, 0), 1, 44, file_out);

    if (info.samples)
    {
        total_samples += info.samples;
        if (buf_ref)
        {
            int max_samples = MINIMP3_MIN((size_t)ref_size/2, info.samples);
            for (i = 0; i < max_samples; i++)
            {
                int MSEtemp = abs((int)buffer[i] - (int)(int16_t)read16le(&buf_ref[i*sizeof(int16_t)]));
                if (MSEtemp > maxdiff)
                    maxdiff = MSEtemp;
                MSE += (float)MSEtemp*(float)MSEtemp;
            }
        }
        if (file_out)
            fwrite(buffer, info.samples, sizeof(int16_t), file_out);
        free(buffer);
    }

#ifndef LIBFUZZER
    MSE /= total_samples ? total_samples : 1;
    if (0 == MSE)
        psnr = 99.0;
    else
        psnr = 10.0*log10(((double)0x7fff*0x7fff)/MSE);
    printf("rate=%d samples=%d max_diff=%d PSNR=%f\n", info.hz, total_samples, maxdiff, psnr);
    if (psnr < 96)
    {
        printf("PSNR compliance failed\n");
        exit(1);
    }
#endif

    if (wave_out && file_out)
    {
        data_bytes = ftell(file_out) - 44;
        rewind(file_out);
        fwrite(wav_header(info.hz, info.channels, 16, data_bytes), 1, 44, file_out);
    }
}

bool myminimp3_decodeFile(char *input_file_name, char *output_file_name) {
    printf("init minimp3\n");
    int wave_out = 0;
    FILE *file_out = NULL;

    if (!input_file_name)
    {
        printf("error: no input file name given\n");
        return false;
    }

    if (!output_file_name)
    {
        printf("error: no output file names given\n");
        return false;
    }

    file_out = fopen(output_file_name, "wb");

    char *ext = strrchr(output_file_name, '.');
    if (ext && !strcasecmp(ext + 1, "wav"))
        wave_out = 1;

    decode_file(input_file_name, NULL, 0, file_out, wave_out);

    if (file_out)
        fclose(file_out);

    return true;
}

