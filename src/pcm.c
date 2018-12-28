#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>

#include "myminimp3.h"

#define PCM_DEVICE "default"

typedef struct _pcm_info_t {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    unsigned int period;
} pcm_info_t;

typedef struct _wav_header_t {
    int16_t ch;
    uint32_t hz;
    int32_t nAvgBytesPerSec;
    int16_t nBlockAlign;
    int16_t bips;
    int32_t data_bytes;
} wav_header_t;

static bool running = false;

static unsigned int buffer_time = 100000;
static unsigned int period_time = 10000;

void pcm_printWavHeader(wav_header_t *myWavHeader) {
    if(!myWavHeader) {
        printf("wavheader is null not printing\n\r");
        return;
    }
    printf("==============================\n\r");
    printf("wavheader is:\n\r");
    printf("ch:%d\n\r", myWavHeader->ch);
    printf("hz:%d\n\r", myWavHeader->hz);
    printf("nAvgBytesPerSec:%d\n\r", myWavHeader->nAvgBytesPerSec);
    printf("nBlockAlign:%d\n\r", myWavHeader->nBlockAlign);
    printf("bips:%d\n\r", myWavHeader->bips);
    printf("data_bytes:%d\n\r", myWavHeader->data_bytes);
    printf("==============================\n\r");
}

void pcm_printPCMState(pcm_info_t *pcm_info) {
    unsigned int tmp = 0;

    printf("PCM name: '%s'\n", snd_pcm_name(pcm_info->pcm_handle));

    printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_info->pcm_handle)));

    snd_pcm_hw_params_get_channels(pcm_info->params, &tmp);
    printf("channels: %i ", tmp);

    if (tmp == 1)
        printf("(mono)\n");
    else if (tmp == 2)
        printf("(stereo)\n");

    snd_pcm_hw_params_get_rate(pcm_info->params, &tmp, 0);
    printf("rate: %d bps\n", tmp);
}

static wav_header_t *pcm_initWavHeader() {
    wav_header_t* myWavHeader = (wav_header_t *) calloc (1, sizeof(wav_header_t));
    return myWavHeader;
}

static void pcm_clearWavHeader(wav_header_t *myWavHeader) {
    free(myWavHeader);
}

static wav_header_t *pcm_readWavHeader(FILE *input_file) {
    wav_header_t* myHeader= NULL;
    char buf[44];
    if(!input_file) {
        printf("error, input_file is NULL\n\r");
        goto exit;
    }

    memset(&buf[0], 0, sizeof(buf));

    if (fread(buf, 1, 44,input_file)!=44) {
        printf("error: could not read first 44 bytes\n\r");
        goto exit;
    }

    myHeader = pcm_initWavHeader();

    myHeader->ch = *(int16_t *)(void*)(buf + 0x16);
    myHeader->hz = *(uint32_t *)(void*)(buf + 0x18);
    myHeader->nAvgBytesPerSec = *(int32_t *)(void*)(buf + 0x1C);
    myHeader->nBlockAlign = *(int16_t *)(void*)(buf + 0x20);
    myHeader->bips = *(int16_t *)(void*)(buf + 0x22);
    myHeader->data_bytes = *(int32_t *)(void*)(buf + 0x28);

exit:
    return myHeader;
}

static bool pcm_loop(pcm_info_t *pcm_info, wav_header_t *myHeader, FILE* audioFile) {
    bool retval = false;

    char *buff = NULL;
    int buff_size;
    unsigned int pcm = 0;

    buff_size = pcm_info->frames * myHeader->ch * 2 /* 2 -> sample size */;
    buff = (char *) malloc(buff_size);

    while (running && fread(buff, 1, buff_size, audioFile) == buff_size) {
        if ((pcm = snd_pcm_writei(pcm_info->pcm_handle, buff, pcm_info->frames)) == -EPIPE) {
            snd_pcm_prepare(pcm_info->pcm_handle);
        } else if (pcm < 0) {
            printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
        }
    }
    running = false;

    free(buff);

    return retval;
}

static void pcm_cleanup(snd_pcm_t* pcm_handle) {
    if (!pcm_handle) {
        return;
    }
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
}

static pcm_info_t pcm_init(wav_header_t *myHeader) {
    pcm_info_t pcm_info;
    unsigned int pcm = 0;
    static int dir = 0;

    if (!myHeader) {
        printf("myHeader is NULL\n\r");
        goto exit;
    }

    snd_pcm_hw_params_alloca(&(pcm_info.params));

    pcm = snd_pcm_open(&(pcm_info.pcm_handle), PCM_DEVICE,SND_PCM_STREAM_PLAYBACK, 0);
    if (pcm < 0) {
        printf("error: Can't open PCM device: %s. %s\n",PCM_DEVICE, snd_strerror(pcm));
        goto exit;
    }

    snd_pcm_hw_params_any(pcm_info.pcm_handle, pcm_info.params);

    pcm = snd_pcm_hw_params_set_access(pcm_info.pcm_handle, pcm_info.params,SND_PCM_ACCESS_RW_INTERLEAVED);
    if (pcm < 0) {
        printf("error: Can't set interleaved mode. %s\n", snd_strerror(pcm));
        goto exit;
    }

    pcm = snd_pcm_hw_params_set_format(pcm_info.pcm_handle, pcm_info.params,SND_PCM_FORMAT_S16_LE);
    if (pcm < 0) {
        printf("error: Can't set format. %s\n", snd_strerror(pcm));
        goto exit;
    }

    pcm = snd_pcm_hw_params_set_channels(pcm_info.pcm_handle, pcm_info.params, myHeader->ch);
    if (pcm < 0) {
        printf("error: Can't set channels number. %s\n", snd_strerror(pcm));
        goto exit;
    }

    pcm = snd_pcm_hw_params_set_rate_near(pcm_info.pcm_handle, pcm_info.params, &(myHeader->hz), 0);
    if (pcm < 0) {
        printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));
        goto exit;
    }

    pcm = snd_pcm_hw_params_set_buffer_time_near(pcm_info.pcm_handle, pcm_info.params, &buffer_time, &dir);
    if (pcm < 0) {
        printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(pcm));
        goto exit;
    }

    pcm = snd_pcm_hw_params_set_period_time_near(pcm_info.pcm_handle, pcm_info.params, &period_time, &dir);
    if (pcm < 0) {
        printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(pcm));
        goto exit;
    }

    snd_pcm_hw_params_get_period_time(pcm_info.params, &(pcm_info.period), &dir);

    snd_pcm_hw_params_get_period_size(pcm_info.params, &(pcm_info.frames), &dir);

    pcm = snd_pcm_hw_params(pcm_info.pcm_handle, pcm_info.params);
    if (pcm < 0) {
        printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));
        goto exit;
    }

exit:
    return pcm_info;
}

bool pcm_play_song(char *input_file) {
    bool retval = false;
    FILE *wavFile = NULL;
    wav_header_t *myWavHeader = NULL;
    pcm_info_t pcm_info;

    if (!input_file || !(*input_file)) {
        printf("Invalid input\n\r");
        goto error;
    }

    wavFile = fopen(input_file, "rb");
    if (!wavFile) {
        printf("Could not open file:%s\n\r", input_file);
        goto error;
    }

    myWavHeader = pcm_readWavHeader(wavFile);
    if (!myWavHeader) {
        printf("Could not read wav header\n\r");
        goto error;
    }

    pcm_info =pcm_init(myWavHeader);
    if (!pcm_info.pcm_handle) {
        printf("Could not init pcm_handle\n\r");
        goto error;
    }

    running = true;
    pcm_loop(&pcm_info, myWavHeader,wavFile);

    retval = true;
error:
    fclose(wavFile);
    wavFile = NULL;
    pcm_clearWavHeader(myWavHeader);
    pcm_cleanup(pcm_info.pcm_handle);
    return retval;
}

bool pcm_stop_song() {
    running = false;
    return true;
}
