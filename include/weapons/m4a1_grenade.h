#ifndef WEAPONS_M4A1_GRENADE_H
#define WEAPONS_M4A1_GRENADE_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/weapon.h"

/// 0x34-byte scratch the flight state takes from `G_SCRATCH_HEAD`. The
/// `GpDeltaScratch` at 0x20 is handed to `func_800E0FEC` and also holds the
/// per-frame translation the state adds onto the projectile coordinate;
/// `sfx` is the attachment id the explosion effect and sound are keyed on.
typedef struct M4a1GrenadeScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ s32            sfx;
} M4a1GrenadeScratch;
STATIC_ASSERT_SIZEOF(M4a1GrenadeScratch, 0x34);

typedef void (*M4a1GrenadeStateFn)(Task* task);

void func_m4a1_grenade_8011D654(Task* arg0);
void func_m4a1_grenade_8011D994(Task* arg0);
void func_m4a1_grenade_8011DE68(Task* arg0);

/// Impact clip id per attachment, indexed by `sfx - 0xA`: 0x1F4, 0x4B0, 0x7D0.
extern u16 D_m4a1_grenade_8012E08C[4];

#endif
