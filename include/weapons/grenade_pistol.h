#ifndef WEAPONS_GRENADE_PISTOL_H
#define WEAPONS_GRENADE_PISTOL_H

#include "common.h"

#include "gameplay/3FB8.h"
#include "main/task.h"

/// 0x38-byte scratch the grenade pistol's flight state takes from
/// `G_SCRATCH_HEAD`. The `GpDeltaScratch` at 0x20 is handed to `func_800E0FEC`
/// and also holds the per-frame translation added onto the projectile
/// coordinate. Unlike the M4A1 grenade, the attachment id is not looked up
/// from the equipped slot: both `field_30` and `sfx` are unpacked from
/// `Task::spawnArg1`, `field_30` keeping the byte above it that seeds the
/// sound bank.
typedef struct GrenadePistolScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ s32            field_30;
    /* 0x34 */ s32            sfx;
} GrenadePistolScratch;
STATIC_ASSERT_SIZEOF(GrenadePistolScratch, 0x38);

/// Impact clip id per attachment, indexed by `sfx - 0xA`.
extern u16 D_grenade_pistol_8012B430[4];

void func_grenade_pistol_8011D6FC(Task* arg0);

#endif
