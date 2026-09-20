#ifndef ACTORS_SHARED_80149E54_H
#define ACTORS_SHARED_80149E54_H

#include "common.h"
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

typedef struct ActorWaveCtx {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
    /* 0x8 */ u8  field_8;
    /* 0x9 */ u8  field_9;
    /* 0xA */ u8  field_A;
    /* 0xB */ u8  field_B;
} ActorWaveCtx;

void ActorsShared80149e54(Task* task);

#endif
