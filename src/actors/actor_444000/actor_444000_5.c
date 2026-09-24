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

/// Reseed every slot of the three even animation members from `field_7B3` when
/// the id it names differs from the latched `field_7B2`, then latch it. Each
/// slot also has its `rate` seeded from `field_7B6`, and the reset argument
/// comes from the `[field_7B2][field_7B3]` transition table.
void func_actor_444000_80134040(Actor444000* arg0)
{
    Actor444000Work* work = arg0->field_1C;
    s32              i;

    if (work->field_7B2 != work->field_7B3) {
        for (i = 1; i < 8; i++) {
            work->slots0[i].rate = work->field_7B6;
            func_800B4114(&work->anim0, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots2[i].rate = work->field_7B6;
            func_800B4114(&work->anim2, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots4[i].rate = work->field_7B6;
            func_800B4114(&work->anim4, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        work->field_7B2 = work->field_7B3;
    }
}
