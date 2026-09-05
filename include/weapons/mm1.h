#ifndef WEAPONS_MM1_H
#define WEAPONS_MM1_H

#include "common.h"

#include "gameplay/3FB8.h"
#include "main/task.h"

/// 0x38-byte scratch the MM1's flight state takes from `G_SCRATCH_HEAD`. The
/// `GpDeltaScratch` at 0x20 is handed to `func_800E0FEC` and also holds the
/// per-frame translation added onto the projectile coordinate. Both
/// `field_30` and `sfx` are unpacked from `Task::spawnArg1`, `field_30`
/// keeping the byte above the attachment id that seeds the sound bank.
typedef struct Mm1Scratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ s32            field_30;
    /* 0x34 */ s32            sfx;
} Mm1Scratch;
STATIC_ASSERT_SIZEOF(Mm1Scratch, 0x38);

/// The overlay's view of the owning `Task`: only the state index the weapon's
/// per-frame dispatcher reads is known so far.
typedef struct Mm1Task {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  state;
} Mm1Task;

typedef void (*Mm1StateFn)(Mm1Task* task);

void func_mm1_8011D704(Task* arg0);
void func_mm1_8011DBD8(Mm1Task* arg0);

#endif
