#include "common.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_801326b4.h"
#include "main/task.h"

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_521100_80136724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i]);
        i++;
    } while (i < 0x13);
}

/// Re-inits animation slots 1..0x12 from `animId`, forcing each slot's set
/// index to 1 first, and latches that id into `field_47E` as the one now
/// playing.
void func_actor_521100_8013677C(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_521100_8016A3D8->slots[i].rate = 1;
        Gp_AnimInitSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i], i,
                        (s16)D_actor_521100_8016A3D8->animId);
        i++;
    } while (i < 0x13);
    D_actor_521100_8016A3D8->field_47E = D_actor_521100_8016A3D8->animId;
}

/// Reseeds animation slots 1..0x12 from `animId` and latches that id into
/// `field_47E` as the one now playing.
void func_actor_521100_80136820(void)
{
    s32 i;

    i = 1;
    do {
        func_800B3AA4(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i], i,
                      (s16)D_actor_521100_8016A3D8->animId, 0, 8);
        i++;
    } while (i < 0x13);
    D_actor_521100_8016A3D8->field_47E = D_actor_521100_8016A3D8->animId;
}

/// Scale-in step body, run while `field_484` is 1: takes a 0x30-byte scratch
/// from `G_SCRATCH_HEAD`, splats an identity rotation into it and hands it to
/// `ScaleMatrix` with a `(0x1000, field_488, 0x1000)` vector, then multiplies
/// the product into the attach coordinate whose rotation step 0 snapshotted
/// into `field_48C`. The scale drops 0x10 a frame; under 0x101 the step
/// advances to 2 and this body stops running.
///
/// The scratch pointer is taken with a chained assignment on purpose: the
/// store and the callee-saved copy are what put the extra `move $s0, $v0`
/// between the `addiu` and the `sw` (and the `nop` in the load's delay slot).
void func_actor_521100_801368B0(Actor521100* arg0)
{
    MATRIX*                  head;
    Actor521100ScaleScratch* scratch;
    Actor521100Work*         work;
    GsCOORDINATE2*           coord;

    head    = *(MATRIX**)0x1F8003FC;
    work    = arg0->field_1C;
    scratch = (*(void**)0x1F8003FC = (Actor521100ScaleScratch*)((u8*)head - 0x30));
    coord   = arg0->field_2C->field_8;
    if ((s16)work->field_488 >= 0x101) {
        work->field_488 = (u16)work->field_488 - 0x10;
    } else {
        work->field_484 = 2;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)(s16)work->field_488;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_48C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}
/// Animation-start handler: seeds the work block's `animId` with
/// `args->animId + 1`, rejecting anything whose incremented id is 0xB or up,
/// and leaves the actor in step 2 with `field_482` cleared before running the
/// step dispatcher.
///
/// The published task is read into a local between the first two stores on
/// purpose: that is where the original evaluates it, and it is what puts the
/// global's `lui`/`lw` ahead of the `li 2` and leaves the `field_482` clear
/// for the call's delay slot.
s32 func_actor_521100_801369B8(Task* task, s32 arg1, Actor521100AnimArgs* args)
{
    Task* dispatcher;

    if (args->animId + 1 < 0xB) {
        D_actor_521100_8016A3D8->animId    = (u16)args->animId + 1;
        dispatcher                         = ActorsShared801326b4Task;
        D_actor_521100_8016A3D8->field_47C = 2;
        D_actor_521100_8016A3D8->field_482 = 0;
        func_actor_521100_80135F2C(dispatcher);
        return 0;
    }
    return -1;
}
