#ifndef ACTORS_SHARED_80149E54_H
#define ACTORS_SHARED_80149E54_H

#include "common.h"
#include "actors/actor.h"
#include "main/task.h"

typedef struct ActorWaveRec {
    /* 0x0 */ s16 phase;
    /* 0x2 */ s16 offset;
    /* 0x4 */ s16 speed;
    /* 0x6 */ s16 pad_6;
} ActorWaveRec;

typedef struct ActorWaveScratch {
    /* 0x00 */ ActorWaveRec rows[30];
    /* 0xF0 */ ActorWaveRec cols[9];
} ActorWaveScratch;

void ActorsShared80149e54(Task* task);

#endif
