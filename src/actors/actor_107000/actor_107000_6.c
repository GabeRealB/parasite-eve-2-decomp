#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_801381b0.h"
#include "actors/actors_shared_80137cf4.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actors_shared_80136614.h"

void ActorsShared8014fda4(Task* arg0);
void func_actor_107000_80135280(Task* arg0, TmdObject* arg1, s32 arg2);
void func_actor_107000_8013560C(Task* arg0, TmdObject* arg1, s32 arg2);
void func_actor_107000_80135C28(Task* arg0);
void func_actor_107000_80137F1C(Task* arg0);
void func_actor_107000_8013844C(Task* arg0);

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

extern u8      D_801153F4;
extern SVECTOR D_actor_107000_8013F5D8;

/// Per-frame mode handler of the specimen. The `D_801153F4` switch is the same
/// one `func_actor_107000_801343C4` runs: mode 1 skips to the tail, mode 2 puts
/// the model in its hidden pose and returns, mode 0 clears both flags and falls
/// into the body. The body first dispatches the reaction sub-state `field_36A` -
/// 0 and 1 hand the frame to their own handler, 3 counts `field_36E` out to 0xB
/// before resyncing the animation and dropping back to 0 once the enemy's flag-2
/// fires, 4 does the count alone, and 5 adds the 0x392 spawn counter whose fifth
/// hit re-cues the impact sound, switches the task to its death state and clears
/// the transform angle. 4 and 5 share the `field_390` timer that spawns a
/// 0x60080 effect every 0x10 frames. The tail then twists the model's second
/// coordinate part, runs the five per-frame helpers, and clears the display
/// flags of the model's first two parts before recomputing the second.
void func_actor_107000_80134F84(Actor107000Ctx* arg0, Task* arg1)
{
    TmdObject*       obj;
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s32              soundId;

    obj   = (TmdObject*)arg1->extra;
    state = D_801153F4;
    work  = (Actor107000Work*)arg1->work;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags     = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->flags     = 0x80;
    arg0->field_14 = one;
    return;
default_body:
    switch (work->field_36A) {
        case 0:
            func_actor_107000_80135280(arg1, obj, one);
            break;
        case 1:
            func_actor_107000_8013560C(arg1, obj, one);
            break;
        case 3:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372 = 2;
                work->field_370 = 5;
                work->field_374 = 0;
                work->field_36E = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)arg1->spawnArg2) != 0) {
                work->field_36A = 0;
            }
            break;
        case 4:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372 = 2;
                work->field_370 = 5;
                work->field_374 = 0;
                work->field_36E = 0;
            }
            goto block_21;
        case 5:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372  = 2;
                work->field_370  = 5;
                work->field_374  = 0;
                work->field_36E  = 0;
                work->field_392 += 1;
                if ((u32)work->field_392 >= 5U) {
                    SndEvt_EnqueueType7(0x40460003, 0);
                    soundId = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460005;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    work->field_2CA &= 0x7FFF;
                    arg1->state      = 2;
                    work->field_36C  = 0;
                }
            }
        block_21:
            work->field_390 += 1;
            if ((u32)work->field_390 >= 0x10U) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg1->extra)->coords, 0x400, &D_actor_107000_8013F5D8);
                work->field_390 = 0;
            }
            break;
    }
    coord->coord.t[1] += 0x80;
    func_actor_107000_8013844C(arg1);
    func_actor_107000_80135C28(arg1);
    ActorsShared80137cf4(arg1);
    func_actor_107000_80137F1C(arg1);
    ActorsShared801381b0(arg1);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    ActorsShared8014fda4(arg1);
}

// actor_207000 carries the same body at 0x8014CF84, but it cannot be shared:
// six of its calls - the two reaction handlers, the three per-frame helpers and
// the spawn-effect helper - plus the `D_actor_107000_8013F5D8` vector it spawns
// with are this overlay's own, each at its own address in the two carriers.
// `overlay_dup_index.py promote` refuses it for that reason; matching it once
// needs those symbols shared first.

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_80135280);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_8013560C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_80135C28);

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
/// `ActorsShared80136614` and picks branch 2 once the target is 2500
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

    enemy      = arg0->spawnArg2;
    obj        = arg0->extra;
    coord      = obj->coords;
    work       = (Actor107000Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp <= 0) {
        SndEvt_EnqueueType7(0x40460003, 0);
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460005;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        work->field_2CA &= 0x7FFF;
        arg0->state      = 2;
        work->field_36C  = 0;
        return;
    }
    SndEvt_EnqueueType7(0x40460003, 0);
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460004;
    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
            ActorsShared80136614(((TmdObject*)arg0->extra)->coords, &sp10);
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

// actor_207000 carries the same body as func_actor_207000_8014E094; the two
// were kept apart only because the body's last call was this overlay's own
// func_actor_107000_80136614. That call now goes to the shared
// ActorsShared80136614 in both slots, so the objection no longer holds.

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000_6", ActorsShared80138404Table);
