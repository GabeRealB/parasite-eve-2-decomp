#ifndef WEAPONS_M4A1_GRENADE_H
#define WEAPONS_M4A1_GRENADE_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

/// 0xA0 work block the grenade's spawn state allocates with `Mem_Calloc` and
/// parks in `Task::idMap`. It leads with the two `GpObj` list nodes
/// `WeaponsShared8011de24` hands back to `Gp_UnlinkObj` on teardown: `obj` is
/// a `flags & 7 == 1` node whose `field_C` is `rec0` directly, `obj2` is a
/// `flags & 7 == 3` node whose `field_C` is `d4rec`, reaching `rec1` through
/// `GpActorD4Rec::field_14`. `field_88` is the flight countdown
/// `WeaponsShared8011ddf8` decrements, and `dir` is the launch direction:
/// column 2 of the muzzle matrix pitched up 0x400, normalized. `field_88` is
/// 16.16: the whole word is the flight timer, while its high half is the
/// per-frame divisor `func_m4a1_grenade_8011D994` steps `dir` by, so the
/// grenade slows as the timer counts up.
typedef struct M4a1GrenadeWork {
    /* 0x00 */ GpObj        obj;
    /* 0x20 */ GpObj        obj2;
    /* 0x40 */ GpRec18      rec0[1];
    /* 0x58 */ GpRec18      rec1[1];
    /* 0x70 */ GpActorD4Rec d4rec;
    /* 0x88 */ GpFixed16    field_88;
    /* 0x8C */ s32          field_8C;
    /* 0x90 */ s32          field_90;
    /* 0x94 */ SVECTOR      dir;
    /* 0x9C */ byte         pad_9C[4];
} M4a1GrenadeWork;
STATIC_ASSERT_SIZEOF(M4a1GrenadeWork, 0xA0);

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

#endif
