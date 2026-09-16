#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80134C2C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80134F84);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80135280);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_8013560C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80135C28);

/// Hit reaction of the specimen. `arg1` comes off the context's HP countdown
/// and is pushed through the lock-slot updater by the same amount. A spent
/// countdown switches the task to its death state (2), clears the transform
/// angle and drops the work out of the pose; a live one cues the impact sound
/// - bits 12+ of the context's `field_8` pick the sound bank - and then walks
/// the reaction sub-state `field_36A` through its wind-up.
///
/// The sub-state is only advanced while it sits below 2: `arg1` at or above
/// 0x33 lands on the long recoil (sub-state 1, animation 4) and 0x15 or above
/// on the short one (sub-state 0, animation 6). Below both, an idle sub-state
/// with no branch selected re-measures the coordinate with
/// `func_actor_107000_80136614` and picks branch 2 once the target is 2500
/// units away, branch 1 otherwise.
void func_actor_107000_80136094(Task* arg0, s32 arg1)
{
    u32              sp10;
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s16              state;

    enemy            = arg0->spawnArg2;
    obj              = arg0->extra;
    coord            = obj->field_8;
    work             = (Actor107000Work*)arg0->idMap;
    enemy->field_40 -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->field_40 <= 0) {
        SndEvt_EnqueueType7(0x40460003, 0);
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40460005;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
        work->field_2CA &= 0x7FFF;
        arg0->state      = 2;
        work->field_36C  = 0;
        return;
    }
    SndEvt_EnqueueType7(0x40460003, 0);
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40460004;
    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    state = work->field_36A;
    if (state < 2) {
        if ((u32)arg1 >= 0x33) {
            work->field_36A = 1;
            work->field_374 = 0;
            work->field_36E = 0;
            work->field_382 = 4;
            return;
        }
        if ((u32)arg1 >= 0x15) {
            work->field_374 = 0;
            work->field_36A = 0;
            work->field_36E = 0;
            work->field_370 = 6;
            return;
        }
        if (state == 0 || work->field_382 == 0) {
            func_actor_107000_80136614((GpObj38*)((TmdObject*)arg0->extra)->field_8, &sp10);
            work->field_36A = 1;
            work->field_36E = 0;
            if (sp10 >= 0x9C4) {
                work->field_382 = 2;
                return;
            }
            work->field_382 = 1;
        }
    }
}

// actor_207000 carries the same body as func_actor_207000_8014E094. Promotion
// is refused: the body's last call is this overlay's own
// func_actor_107000_80136614, which actor_207000 links as its own
// func_actor_207000_8014E614.

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136288);

// actor_207000 carries the same body as func_actor_207000_8014E4D8. Promotion
// is refused for the same reason as func_actor_107000_80136288 above: the body
// calls this overlay's own func_actor_107000_80136614, which actor_207000 links
// as its own func_actor_207000_8014E614.

/// Both `Gp_LcgState` draws fold `(state >> 16) % 100` down to under 11 for a
/// hit, and each is taken through its own local so the two divides stay separate
/// objects: the compiler keeps the second draw's quotient alive across the
/// scaling chain, and `SCHED_BARRIER` holds the reaction branch's assignment on
/// the far side of it. The two barriers are load-bearing, not decoration —
/// dropping either one re-schedules the `branch` write to before the chain and
/// the whole block's allocation follows it.
void func_actor_107000_801364D8(Task* arg0)
{
    u32              sp10;
    Actor107000Work* work;
    u32              rng;
    u32              rng2;
    u32              hi;
    u32              quotient;
    u32              roll;
    s32              hit;
    s16              branch;

    work = (Actor107000Work*)arg0->idMap;
    func_actor_107000_80136614(((TmdObject*)arg0->extra)->field_8, &sp10);
    if (Gp_CountRec18Hi(&work->field_214, 0x10000) == 0 || sp10 >= 0xBB8U) {
        work->field_382 = 0;
        work->field_36E = 0;
    } else {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if ((u16)((rng >> 0x10) % 100U) < 0xBU) {
            branch = 3;
        } else if (sp10 >= 0x9C4U) {
            branch = 2;
        } else {
            rng2        = rng * 5 + 0x71357911;
            Gp_LcgState = rng2;
            hi          = rng2 >> 0x10;
            quotient    = hi / 100U;
            SCHED_BARRIER();
            roll = (u16)(hi - quotient * 100U);
            SCHED_BARRIER();
            hit    = (roll < 0xBU);
            branch = 2;
            if (!hit) {
                branch = 1;
            }
        }
        work->field_382 = branch;
        work->field_374 = 0;
        work->field_372 = 0;
    }
    Gp_ClearRec18Occupied(&work->field_214);
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136614);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136938);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136C80);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136E88);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137220);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_8013777C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_801378D8);

void func_actor_107000_80137C8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_107000_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

// func_actor_107000_80137CF4 is shared with actor_207000; see
// src/actors/lib/actors_shared_80137cf4.c.
