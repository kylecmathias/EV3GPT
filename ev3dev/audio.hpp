#pragma once

#include "config.hpp"
#include "connection.hpp"
#include <alsa/asoundlib.h>

inline constexpr const char AUDIO_DEVICE[] = "default";
inline constexpr size_t AUDIO_QUEUE_SIZE = 32;

struct AudioPacket {
    uint8_t control;
    uint8_t payload[AUDIO_PAYLOAD_SIZE];
};

struct AudioQueue {
    AudioPacket packets[AUDIO_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

void audio_queue_init(AudioQueue *queue); //initialize audio queue of size AUDIO_QUEUE_SIZE
bool audio_queue_push(AudioQueue *queue, const AudioPacket *packet); //add item to end of audio queue
bool audio_queue_pop(AudioQueue *queue, AudioPacket *packet); //get next item in audio queue
void* audio_receiver(void* arg); 
void* audio_player(void* arg);