#include "audio.hpp"

extern std::atomic<bool> running;

static AudioQueue audio_queue;
static uint8_t current_priority = 0;

void audio_queue_init(AudioQueue *queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

bool audio_queue_push(AudioQueue *queue, const AudioPacket *packet) {
    pthread_mutex_lock(&queue->mutex);

    uint8_t incoming_priority = (packet->control & AUDIO_PRIORITY_MASK) >> AUDIO_PRIORITY_SHIFT;

    if (packet->control & AUDIO_FLAG_INTERRUPT) {
        if (incoming_priority >= current_priority) {
            queue->head = 0;
            queue->tail = 0;
            queue->count = 0;
            current_priority = incoming_priority;
        }
        else {
            pthread_mutex_unlock(&queue->mutex);
            return false;
        }
    }

    if (queue->count >= AUDIO_QUEUE_SIZE) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    queue->packets[queue->tail] = *packet;
    queue->tail = (queue->tail + 1) % AUDIO_QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

bool audio_queue_pop(AudioQueue *queue, AudioPacket *packet) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0 && running.load()) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    if (!running.load()) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    *packet = queue->packets[queue->head];
    queue->head = (queue->head + 1) % AUDIO_QUEUE_SIZE;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);
    return true;
}

void* audio_receiver(void* arg) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("audio socket");
        return NULL;
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(AUDIO_PORT);

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("audio bind");
        close(sockfd);
        return NULL;
    }

    AudioPacket packet;

    while (running.load()) {
        ssize_t len = recv(sockfd, &packet, sizeof(AudioPacket), 0);

        if (len == sizeof(AudioPacket)) {
            if (packet.control & AUDIO_FLAG_FLUSH) {
                pthread_mutex_lock(&audio_queue.mutex);
                audio_queue.head = 0;
                audio_queue.tail = 0;
                audio_queue.count = 0;
                current_priority = 0;
                pthread_mutex_unlock(&audio_queue.mutex);
                continue;
            }
            audio_queue_push(&audio_queue, &packet);
        }
    }

    close(sockfd);
    return NULL;
}

void* audio_player(void* arg) {
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *params;

    if (snd_pcm_open(&pcm, AUDIO_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        perror("ALSA open");
        return NULL;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm, params);
    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, params, AUDIO_CHANNELS);

    unsigned int rate = AUDIO_SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(pcm, params, &rate, 0);
    snd_pcm_hw_params(pcm, params);

    audio_queue_init(&audio_queue);

    AudioPacket packet;

    while (running.load()) {
        if (!audio_queue_pop(&audio_queue, &packet)) break;

        if (packet.control & AUDIO_FLAG_END) {
            current_priority = 0;
        }

        //frames = bytes / (channels * bytes_per_sample)
        snd_pcm_uframes_t frames = AUDIO_PAYLOAD_SIZE / (AUDIO_CHANNELS * 2);
        snd_pcm_sframes_t written = snd_pcm_writei(pcm, packet.payload, frames);

        if (written == -EPIPE) {
            snd_pcm_prepare(pcm); 
        } else if (written < 0) {
            snd_pcm_recover(pcm, written, 0);
        }
    }

    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
    return NULL;
}