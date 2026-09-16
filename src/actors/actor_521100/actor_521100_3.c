#include "common.h"

#include "actors/actor_521100.h"

#include <psyq/abs.h>

void func_actor_521100_801358D4(Actor521100* arg0);
void func_actor_521100_80135964(Actor521100* arg0);
void func_actor_521100_80132958(void);
void func_actor_521100_80132DE8(void);
void func_actor_521100_801339B0(void);
void func_actor_521100_8013570C(Actor521100* arg0);
void func_actor_521100_80134658(Actor521100* arg0);
void func_actor_521100_801357F0(Actor521100* arg0);
void func_actor_521100_80134774(void);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_521100_801355C8(Actor521100* arg0)
{
    s16 temp_v1;

    temp_v1 = arg0->field_1C->field_69E;
    switch (temp_v1) {
        case 0:
            func_actor_521100_80132958();
            return;
        case 1:
            func_actor_521100_80132DE8();
            return;
        case 2:
            func_actor_521100_801339B0();
            return;
        case 3:
            func_actor_521100_8013570C(arg0);
            return;
        case 4:
            func_actor_521100_80134658(arg0);
            return;
        case 5:
            func_actor_521100_801357F0(arg0);
            return;
        case 6:
            func_actor_521100_80134774();
        default:
            return;
    }
}

/// Steps the actor into state 1 once its facing has come within 45 degrees of
/// the angle at `field_696`, then stops it: both speeds are zeroed.
void func_actor_521100_80135680(Actor521100* arg0)
{
    Actor521100Work* work;
    s16              delta;
    s16              angle;
    s16              wrapped;
    s32              magnitude;

    work      = arg0->field_1C;
    delta     = work->field_698 - work->field_696;
    magnitude = abs(delta);
    if (magnitude < 0x800) {
        angle = magnitude;
    } else {
        if (delta > 0) {
            wrapped = 0x1000 - delta;
        } else {
            wrapped = delta + 0x1000;
        }
        angle = wrapped;
    }
    if ((angle < 0x200) && (work->field_6AA < 0xDAC)) {
        work->field_69E = 1;
        work->field_6A0 = 0;
        work->field_69A = 0;
        work->field_69C = 0;
    }
}

/// Step-3 body of the burn-out sequence, the third of the three the dispatcher
/// `func_actor_521100_801355C8` runs off `field_69E`. `field_6A0` is its own
/// two-phase latch: phase 0 hands the record flags at 0x59A / 0x5BA back and
/// asks the slot blend for clip 0x10, phase 1 waits out 0x37 blended frames and
/// then either drops the actor to the idle state or, when `field_6BA` asks for
/// it, on to state 6 at sub-state `field_6BC`. Either way it latches clip 1 for
/// the blend and picks this frame's effect out of `D_actor_521100_8015F634`.
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_3", func_actor_521100_8013570C);
/// Step-5 body of the burn-out sequence, the same two-phase `field_6A0` latch
/// `func_actor_521100_8013570C` runs with the longer timing: phase 0 hands the
/// record flags at 0x59A / 0x5BA back and asks the slot blend for clip 0x11,
/// phase 1 waits out 0x48 blended frames and then either drops the actor to the
/// idle state or, when `field_6BA` asks for it, on to state 6 at sub-state
/// `field_6BC`. Either way it latches clip 1 for the blend and picks this
/// frame's effect out of `D_actor_521100_8015F5F4`.
INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_3", func_actor_521100_801357F0);
/// Snapshots the attach coordinate's translation into the work block, then
/// walks the coordinate forward: 0x80 up, and along its own facing axis
/// (`m[0][2]` / `m[2][2]`) scaled by the work block's speed in 12-bit fixed
/// point. The same body as `ActorsShared80134f60`, on this actor's own
/// coordinate.
void func_actor_521100_801358D4(Actor521100* arg0)
{
    GsCOORDINATE2*   coord;
    Actor521100Work* work;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_64C    = coord->coord.t[0];
    work->field_64E    = coord->coord.t[1];
    work->field_650    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_69A) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_69A) >> 12;
}

/// Blends every animation slot towards the clip latched in `field_686` while
/// it differs from the clip the slots carry, then ticks them once they agree:
/// the blend runs the nineteen slots through `func_800B4114` with the length
/// `D_actor_521100_8015F894` gives the incoming clip, and the tick counts the
/// agreeing frames in `field_68A`.
void func_actor_521100_80135964(Actor521100* arg0)
{
    Actor521100Work* work;
    s32              i;
    s32              val;

    work = arg0->field_1C;
    val  = 0;
    if (work->field_686 != work->field_688) {
        work->field_688 = work->field_686;
        work->field_68A = 0;
        if (work->field_686 < 0x15) {
            val = D_actor_521100_8015F894[work->field_686];
        }
        i = 1;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_686, 0, val);
            i++;
        } while (i < 0x13);
        return;
    }
    i                = 1;
    work->field_68A += i;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 0x13);
}