#ifndef M4A1_JAVELIN_H
#define M4A1_JAVELIN_H

#include "common.h"
#include <psyq/libgte.h>

/// Low halves of a `VECTOR3` (typically `GsCOORDINATE2.workm.t`).
typedef struct M4a1JavelinVecLo {
    /* 0x0 */ u16  vx;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ u16  vy;
    /* 0x6 */ byte pad_6[2];
    /* 0x8 */ u16  vz;
} M4a1JavelinVecLo;

extern SVECTOR D_m4a1_javelin_8012EB68;
extern s32     D_m4a1_javelin_8012EB70;

void func_m4a1_javelin_8011F4A4(M4a1JavelinVecLo* arg0);

#endif
