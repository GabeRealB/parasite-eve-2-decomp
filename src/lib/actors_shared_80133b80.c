#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actor_444000_view.h"
#include "actors/actors_shared_80133de4.h"
#include "actors/actors_shared_80133f64.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/abs.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Global freeze flag: 1 while the game is halted, which stops the run below
/// from advancing the model.

/// Which of the three drop-point groups the falling enemies use this round,
/// rerolled off `Gp_LcgState` whenever a spawn arrives with `spawnArg1` 0.
/// Per-`spawnArg1` offset from the host model to the point the enemy is stood
/// up at when it is spawned.
/// The drop points themselves: `vz` is added to the ring x coordinate and `vx`
/// (less 0x189C) becomes the z coordinate.
/// `[group][spawnArg1]` index into `D_actor_444000_80161744`.
extern u8 D_actor_444000_801617C4[][8];
/// Reply buffer the hold state below hands message 0x3F8.

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.

/// Per-animation reset argument, a `[?][0x2D]` table of `field_7B3` indexed by
/// the id that was playing before the switch.
extern s8 D_actor_444000_80160C5C[][0x2D];

void ActorsShared80133b80(Actor444000* arg0)
{
    GpAnimPose       pose0;
    GpAnimPose       pose1;
    Actor444000Work* work     = arg0->field_1C;
    s32              blend    = work->field_7C0;
    s32              invBlend = 0x1000 - blend;
    s16              i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->slots1[i].rate = work->field_7BE;
            work->slots0[i].rate = work->field_7B6 - 3;
            func_800B3448(&work->anim0, i, (s32)&pose0, 0);
            func_800B3448(&work->anim1, i, (s32)&pose1, 0);
            Gp_AnimWritePoseCopy(&work->anim0, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->slots0[i].rate = work->field_7B6 - 3;
            Gp_AnimTickIndex(&work->anim0, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->slots3[i].rate = work->field_7BE;
        work->slots2[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim2, i, (s32)&pose0, 0);
        func_800B3448(&work->anim3, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim2, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->slots5[i].rate = work->field_7BE;
        work->slots4[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim4, i, (s32)&pose0, 0);
        func_800B3448(&work->anim5, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim4, i, &pose0, &pose1, blend, invBlend);
    }
}
