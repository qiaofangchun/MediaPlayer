//
// Created by admin on 2022/11/25.
//

#ifndef HOLA_PACKET_QUEUE_H
#define HOLA_PACKET_QUEUE_H

#include <queue>
#include <pthread.h>
#include "play_status.h"
#include "consts.h"
#include <malloc.h>

extern "C" {
#include "libavcodec/avcodec.h"
};

class PacketQueue {
public:
    PacketQueue(PlayStatus *play_status);

    ~PacketQueue();

public:
    std::queue<AVPacket *> packet_queue;
    pthread_mutex_t packet_mutex;
    pthread_cond_t packet_cond;
    PlayStatus *play_status;

public:
    int push(AVPacket *avPacket);

    int pop(AVPacket *av_packet);

    bool empty();

    void clear();
};


#endif //HOLA_PACKET_QUEUE_H
