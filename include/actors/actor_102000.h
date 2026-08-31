#ifndef ACTOR_102000_H
#define ACTOR_102000_H

#include "common.h"

typedef struct Actor02000Work {
    /* 0x000 */ byte pad_0[0x694];
    /* 0x694 */ s16  field_694;
    /* 0x696 */ byte pad_696[2];
    /* 0x698 */ s16  field_698;
    /* 0x69A */ byte pad_69A[0xC];
    /* 0x6A6 */ s16  field_6A6;
    /* 0x6A8 */ s16  field_6A8;
    /* 0x6AA */ byte pad_6AA[4];
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /* 0x6B2 */ s16  field_6B2;
} Actor02000Work;

typedef struct Actor02000 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor02000Work* field_1C;
} Actor02000;

#endif
