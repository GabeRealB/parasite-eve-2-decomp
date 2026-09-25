#ifndef ACTOR_161500_H
#define ACTOR_161500_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"

extern u8 D_80072729;

/// Work block the actor's spawn routine allocates with `memCalloc(0x4FC, 0)`
/// and hangs off its task's `Task::work` slot. `light` / `color` are the
/// matrix pair the actor's model and its sub-model are lit with, and `anim`,
/// `slots` and `pose` are what `func_800B3F84` fills in.
///
/// The spawn routine optionally spawns a paired enemy, parks that spawn's task
/// in `pairTask` and reparents its own task under it, so the two share one
/// sub-model chain; `enemy` is the enemy its own task belongs to.
///
/// `state` selects what the step body does next: 1 and 2 reseed the animation
/// slots from `animId` (with and without `animArg`) and advance to 3, which
/// ticks them. `appliedAnimId` records the id the slots were last seeded with.
/// `animId` 4 is the walk clip, which `travel` counts down in steps of 30.
/// `yaw` caches the heading the placement and walk-to opcodes last gave the
/// root coordinate. `field_4F0` is the weight, 0 to 0x1000, of the per-frame
/// head turn toward the player, ramped up while `field_4EE` is 1 and down
/// otherwise.
typedef struct Actor161500Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pose[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ byte       pad_4BA[0x2C];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ s16        field_4F0;
    /* 0x4F2 */ byte       pad_4F2[0x2];
    /* 0x4F4 */ Task*      pairTask;
    /* 0x4F8 */ GpEnemy*   enemy;
} Actor161500Work;
STATIC_ASSERT_SIZEOF(Actor161500Work, 0x4FC);

/// Payload of the script opcode that writes the work block's `field_4EE`.
typedef struct Actor161500FlagArgs {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  value;
} Actor161500FlagArgs;

/// Payload of the walk-to opcode: the world position to walk to. Only the
/// horizontal components are read.
typedef struct Actor161500WalkTarget {
    /* 0x00 */ VECTOR pos;
} Actor161500WalkTarget;

/// Payload of the placement opcode: a world translation followed by Euler
/// angles, of which only the yaw (`rot.vy`) is used.
typedef struct Actor161500Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor161500Placement;

/// Steps `coord` `amount` units along its local Z axis unless movement is
/// frozen. The direction is staged on the scratchpad stack.
static __inline__ void Actor161500_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 == 1) {
        return;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    Gfx_MatrixCol2(&coord->coord, vec);
    VectorNormalSS(vec, vec);
    gte_lddp(amount);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    coord->coord.t[0]          += head[-1].vx;
    coord->coord.t[1]          += vec->vy;
    coord->coord.t[2]          += vec->vz;
    coord->flg                  = 0;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}

void func_actor_161500_80132394(GpEnemy* enemy, Task* task);
void func_actor_161500_8013252C(Task* task);
void func_actor_161500_8013273C(GpEnemy* enemy, Task* task);
void func_actor_161500_8013284C(Task* task);
void func_actor_161500_80132874(Task* task);
void func_actor_161500_80132900(Task* task);
void func_actor_161500_8013294C(Task* task);
void func_actor_161500_801329C4(Task* task);

#endif
