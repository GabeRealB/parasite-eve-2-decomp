#ifndef ACTORS_SHARED_801339C0_H
#define ACTORS_SHARED_801339C0_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Work fields used by the falling specimen handler in its four carriers.
typedef struct ActorsShared801339c0Work {
    /* 0x000 */ GpAnimCtx context;
    /* 0x014 */ u8        pad_14[0x29E];
    /* 0x2B2 */ s16       field_2B2;
    /* 0x2B4 */ u8        pad_2B4[4];
    /* 0x2B8 */ s16       field_2B8;
    /* 0x2BA */ s16       field_2BA;
    /* 0x2BC */ u16       field_2BC;
    /* 0x2BE */ s16       field_2BE;
    /* 0x2C0 */ u8        pad_2C0[8];
    /* 0x2C8 */ s16       field_2C8;
    /* 0x2CA */ u8        pad_2CA[8];
    /* 0x2D2 */ s16       field_2D2;
    /* 0x2D4 */ u8        pad_2D4[10];
    /* 0x2DE */ s16       field_2DE;
    /* 0x2E0 */ s16       field_2E0;
    /* 0x2E2 */ s16       field_2E2;
} ActorsShared801339c0Work;

STATIC_ASSERT_SIZEOF(ActorsShared801339c0Work, 0x2E4);

void ActorsShared801339c0(GpEnemy* enemy, Task* task);

#endif
