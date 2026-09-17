#ifndef ACTORS_SHARED_80133830_H
#define ACTORS_SHARED_80133830_H

#include "common.h"

/// Per-actor work block behind `ActorsShared80133830Actor::field_1C`.
/// `field_13E` is the sub-state and `field_140` its frame counter;
/// `field_12A` swings between 0xF00 and 0x1100 in steps of 0x80, with
/// `field_142` holding the direction.
typedef struct ActorsShared80133830Work {
    /* 0x000 */ byte pad_0[0xDE];
    /* 0x0DE */ u16  field_DE;
    /* 0x0E0 */ byte pad_E0[0x48];
    /* 0x128 */ s16  field_128;
    /* 0x12A */ s16  field_12A;
    /* 0x12C */ s16  field_12C;
    /* 0x12E */ byte pad_12E[6];
    /* 0x134 */ s16  field_134;
    /* 0x136 */ byte pad_136[6];
    /* 0x13C */ s16  field_13C;
    /* 0x13E */ s16  field_13E;
    /* 0x140 */ s16  field_140;
    /* 0x142 */ s16  field_142;
    /* 0x144 */ byte pad_144[8];
    /* 0x14C */ s16  field_14C;
} ActorsShared80133830Work;

typedef struct ActorsShared80133830Actor {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorsShared80133830Work* field_1C;
} ActorsShared80133830Actor;

extern u32 Gp_LcgState;

void ActorsShared80133830(ActorsShared80133830Actor* arg0);

#endif
