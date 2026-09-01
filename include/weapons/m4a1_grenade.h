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
/// column 2 of the muzzle matrix pitched up 0x400, normalized.
typedef struct M4a1GrenadeWork {
    /* 0x00 */ GpObj        obj;
    /* 0x20 */ GpObj        obj2;
    /* 0x40 */ GpRec18      rec0[1];
    /* 0x58 */ GpRec18      rec1[1];
    /* 0x70 */ GpActorD4Rec d4rec;
    /* 0x88 */ s32          field_88;
    /* 0x8C */ s32          field_8C;
    /* 0x90 */ s32          field_90;
    /* 0x94 */ SVECTOR      dir;
    /* 0x9C */ byte         pad_9C[4];
} M4a1GrenadeWork;
STATIC_ASSERT_SIZEOF(M4a1GrenadeWork, 0xA0);

typedef void (*M4a1GrenadeStateFn)(Task* task);

void func_m4a1_grenade_8011D654(Task* arg0);
void func_m4a1_grenade_8011D994(Task* arg0);
void func_m4a1_grenade_8011DE68(Task* arg0);

#endif
