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

extern s16 D_actor_444000_80144A68;
extern s32 D_actor_444000_80144A74;
extern s32 D_actor_444000_80144A7C;
extern s32 D_actor_444000_80144A6C;
extern s16 D_actor_444000_80144A70;
extern s32 Gp_LcgState;

extern MATRIX* D_80073B8C;
extern s8      D_8007218A;
extern u8      D_80073BA9;
extern u8      D_801153F4;
/// Global freeze flag: 1 while the game is halted, which stops the run below
/// from advancing the model.
extern u8         D_80072729;
extern GpAnimSet* D_actor_444000_80161694[];

/// Which of the three drop-point groups the falling enemies use this round,
/// rerolled off `Gp_LcgState` whenever a spawn arrives with `spawnArg1` 0.
extern u8 D_actor_444000_80161690;
/// Per-`spawnArg1` offset from the host model to the point the enemy is stood
/// up at when it is spawned.
extern SVECTOR D_actor_444000_80161704[];
/// The drop points themselves: `vz` is added to the ring x coordinate and `vx`
/// (less 0x189C) becomes the z coordinate.
extern SVECTOR D_actor_444000_80161744[];
/// `[group][spawnArg1]` index into `D_actor_444000_80161744`.
extern u8 D_actor_444000_801617C4[][8];
/// Reply buffer the hold state below hands message 0x3F8.
extern Actor444000Msg3F8 D_actor_444000_80161898;
extern GpAnimBlk*        Gp_PlayerAnimBlkTbl[];
extern u16               Gp_WeaponIdBase[];

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_actor_444000_80133010(Actor444000* task);
void func_actor_444000_80133C58(Actor444000* task, s16 arg1);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Per-animation reset argument, a `[?][0x2D]` table of `field_7B3` indexed by
/// the id that was playing before the switch.
extern s8 D_actor_444000_80160C5C[][0x2D];

/// Per-frame animation step. `field_7B0` 1 re-seeds the block through
/// `func_actor_444000_80134040`, 2 resets every slot of the three even members
/// from `field_7B3` outright; either way the block is armed (`field_7B0` 3, the
/// frame counter and the 0x20-byte scratch at `field_7D0` cleared). Then the
/// slots are advanced: plainly while `field_7B1` is clear, otherwise through the
/// blended path, which clears `field_7B1` again once the first pair's slot 1
/// reports done. The three trailing flags run the shared reaction helpers.
void func_actor_444000_8013441C(Actor444000* arg0)
{
    Actor444000Work* work = arg0->field_1C;
    Actor444000Work* w;
    s32              i;

    if (work->field_7B0 == 1) {
        func_actor_444000_80134040(arg0);
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    } else if (work->field_7B0 == 2) {
        w = arg0->field_1C;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim0, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim2, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim4, i, w->field_7B3);
        }
        w->field_7B2    = w->field_7B3;
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    }

    if (work->field_7BA == 2) {
        ActorsShared80133f64((Task*)arg0);
        work->field_7BA = 3;
    }

    work->field_7B4++;

    if (work->field_7B1 == 0) {
        w = arg0->field_1C;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim0, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim2, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim4, i);
        }
    } else {
        func_actor_444000_801341C4(arg0);
        if (work->slots1[1].flags & 1) {
            work->field_7B1 = 0;
        }
    }

    if (work->field_EF4 != 0) {
        ActorsShared80133de4((Task*)arg0, work->field_EFE);
    }
    if (work->field_EF6 != 0) {
        func_actor_444000_80133C58(arg0, work->field_7C4);
    }
    if (work->field_EF8 != 0) {
        func_actor_444000_80133010(arg0);
    }
}
