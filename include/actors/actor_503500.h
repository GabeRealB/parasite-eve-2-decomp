#ifndef ACTOR_503500_H
#define ACTOR_503500_H

#include "common.h"

#include "gameplay/3A34.h"

typedef struct Actor503500Work {
    /* 0x000 */ byte pad_0[0xED];
    /* 0x0ED */ s8   field_ED;
    /* 0x0EE */ byte pad_EE[0x2];
    /* 0x0F0 */ s8   field_F0;
    /* 0x0F1 */ byte pad_F1[0x6B];
    /* 0x15C */ s8   field_15C;
    /* 0x15D */ byte pad_15D[0xC4];
    /* 0x221 */ s8   field_221;
    /* 0x222 */ byte pad_222[0x58E];
    /* 0x7B0 */ s16  field_7B0;
    /* 0x7B2 */ u16  field_7B2;
    /* 0x7B4 */ byte pad_7B4[0x16];
    /* 0x7CA */ s16  field_7CA;
    /* 0x7CC */ byte pad_7CC[0x14];
    /* 0x7E0 */ s8   field_7E0;
} Actor503500Work;

typedef struct Actor503500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor503500Work* field_1C;
    /* 0x20 */ GpObj5D*         field_20;
} Actor503500;

void func_actor_503500_8013F8AC(Actor503500* arg0);
void func_actor_503500_801440F0(Actor503500* arg0);

#endif
