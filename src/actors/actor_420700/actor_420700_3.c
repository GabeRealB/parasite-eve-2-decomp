#include "common.h"
#include "main/task.h"
#include "actors/actor_420700.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_420700_801325C8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->field_4B8, 0, 8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_4B6 = ActorsShared80131f9cWork->field_4B8;
}

s32 func_actor_420700_80132644(Task* task, s32 arg1, Actor420700Msg7D3* args)
{
    s32              offset;
    Actor420700Work* work;

    if (args->field_4 < 0x15) {
        switch (args->field_0) {
            case 1:
                offset = 0xA;
                break;
            case 2:
                offset = 0x11;
                break;
            default:
                offset = 0;
                break;
        }
        work            = ActorsShared80131f9cWork;
        work->field_4B8 = (u16)args->field_4 + offset;
        if (args->field_8 != 0) {
            work->field_4B4 = 1;
        } else {
            work->field_4B4 = 2;
        }
        ActorsShared80131f9cWork->field_4BE = 0;
        func_actor_420700_80132478(D_actor_420700_8013EFE4);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: rewrites `field_C` of every model object the state-0
/// handler owns -- the actor's own task, its model task and the frame-8 twin.
/// Bit 0 of the argument picks the value all three are set to, 0 or the 0x80
/// `Tmd_Create` leaves behind; bit 1 then ORs 0x4 into all three, the same flag
/// `Gp_ApplyAreaTmdFlags` sets. Always returns 0.
///
/// The argument is the handler table's third slot, not the second, so the three
/// objects it loads land in `$a3` / `$a0` / `$v1` rather than shifted one down.
s32 func_actor_420700_801326F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* actor = D_actor_420700_8013EFE4->extra;
    TmdObject* model = D_actor_420700_8013EFE8->extra;
    TmdObject* twin  = D_actor_420700_8013EFEC->extra;

    if (arg2 & 1) {
        actor->flags = 0;
        model->flags = 0;
        twin->flags  = 0;
    } else {
        actor->flags = 0x80;
        model->flags = 0x80;
        twin->flags  = 0x80;
    }
    if (arg2 & 2) {
        actor->flags |= 4;
        model->flags |= 4;
        twin->flags  |= 4;
    }
    return 0;
}

/// Message 0x7DB handler: records the `field_4BA` mode the ramp
/// `ActorsShared80131f9cSub1` runs and seeds `field_4BC` at the end that mode
/// walks away from -- 0 for the rising modes 1 and 3, 0x1000 for the falling
/// mode 2. Mode 0 is accepted as a no-op, and a block whose leading id is not
/// 0x1B02 is rejected with -1 without touching the work block.
///
/// The empty `case 0` is what the decision tree is built from: with the three
/// live cases alone GCC balances the list at the middle node and comes out one
/// test short, and adding the fourth node is what makes it split at the first
/// case instead. See DECOMPILATION_LEARNINGS.md, "An empty case node changes
/// the switch decision tree".
s32 func_actor_420700_80132784(Task* task, s32 arg1, Actor420700ModeArgs* args)
{
    if (args->id != 0x1B02) {
        return -1;
    }
    ActorsShared80131f9cWork->field_4BA = args->mode;
    switch (args->mode) {
        case 0:
            break;
        case 1:
        case 3:
            ActorsShared80131f9cWork->field_4BC = 0;
            break;
        case 2:
            ActorsShared80131f9cWork->field_4BC = 0x1000;
            break;
    }
    return 0;
}

/// `func_actor_420700_801323D8`'s frame-8 twin: the same model-task state
/// handler, hanging this model's root coordinate off a frame of the actor's own
/// model (`D_actor_420700_8013EFE4`) rather than off its own, and dropping that
/// frame's y by 0x320 before handing it to `func_800D7A9C` as the part colour
/// matrix. This one attaches to frame 8 instead of frame 4, and its spawn tick
/// also marks the model's `field_E` as -2.
void func_actor_420700_801327EC(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_420700_8013EFE4->extra)->coords;
    GsCOORDINATE2* part  = parts + 8;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg      = 0;
            extra->flags    = 0;
            extra->otOffset = -2;
            coord->sub      = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}
