#ifndef ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
#define ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

extern s32 Gp_LcgState;

/// Scale applied to held-object reflections in slots 2 and up.
extern const VECTOR D_acropolis_west_elevator_hall_8017D5C4;

/// Index of the mirror model's coordinate part each held-object reflection is
/// parented to, by the reflection's `spawnArg1`.
extern u8 D_acropolis_west_elevator_hall_801802A4[];

/// The mirror's task table: entry 0 runs the mirror itself, entry 1 a
/// held-object reflection.
extern TaskDesc D_acropolis_west_elevator_hall_801802A8[];

/// The hall's two elevator-car tasks, spawned by the room task.
extern Task* D_acropolis_west_elevator_hall_80186AE4[];

void func_acropolis_west_elevator_hall_8017D5FC(Task* task);
void func_acropolis_west_elevator_hall_8017D7B0(Task* task);
void func_acropolis_west_elevator_hall_8017F134(Task* task);
void func_acropolis_west_elevator_hall_8017F304(Task* task);
s32  func_acropolis_west_elevator_hall_8017F470(void);
s32  func_acropolis_west_elevator_hall_8017F498(void);

/// 0x14 scratch block `func_acropolis_west_elevator_hall_8017FFE4` takes from
/// `G_SCRATCH_HEAD` to project one billboard sprite. `pos` is the effect
/// coordinate's `workm` translation truncated to s16 and fed to the `RTPS`,
/// `sx` / `sy` receive the projected centre and `otz` is `SZ3 >> 2`; anything
/// nearer than `otz == 0x11` is dropped instead of drawn. `radius` is the
/// perspective-divided half-extent `0x6700 / otz`, stored as a word but only
/// ever read back as its low halfword.
typedef struct AwehSpriteScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     radius;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ u16     sx;
    /* 0x12 */ u16     sy;
} AwehSpriteScratch;
STATIC_ASSERT_SIZEOF(AwehSpriteScratch, 0x14);

#endif
