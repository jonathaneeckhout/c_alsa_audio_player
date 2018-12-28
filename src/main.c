#include <stdio.h>
#include <stdlib.h>
#include "myminimp3.h"
#include "pcm.h"
#include <signal.h>

void intHandler(int dummy) {
    pcm_stop_song();
}

int main() {
    signal(SIGINT, intHandler);
    myminimp3_decodeFile("lala.mp3", "lala.wav");
    pcm_play_song("lala.wav");
    return 0;
}
