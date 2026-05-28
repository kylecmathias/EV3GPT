#ifndef AUDIO_H
#define AUDIO_H

#include "config.h"
#include "connection.h"
#include <alsa/asoundlib.h>

#define AUDIO_DEVICE "default"
#define AUDIO_QUEUE_SIZE 32

typedef struct {
    uint8_t control;
    uint8_t payload[AUDIO_PAYLOAD_SIZE];
} AudioPacket;

typedef struct {
    AudioPacket packets[AUDIO_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} AudioQueue;

void audio_queue_init(AudioQueue *queue); //initialize audio queue of size AUDIO_QUEUE_SIZE
bool audio_queue_push(AudioQueue *queue, const AudioPacket *packet); //add item to end of audio queue
bool audio_queue_pop(AudioQueue *queue, AudioPacket *packet); //get next item in audio queue
void* audio_receiver(void* arg);
void* audio_player(void* arg);

#endif /* #ifndef AUDIO_H */