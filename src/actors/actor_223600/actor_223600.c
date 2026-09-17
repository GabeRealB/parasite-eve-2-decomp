#include "common.h"

#include "actors/actor_223600.h"
#include "actors/actors_shared_80134178.h"
#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014A170);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014A4B8);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014AA04);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014ABA8);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B2F4);

/// In motion states 2 and 3, reports 0x400C0001 the first time the animation id
/// in `field_4A` reaches one of that state's trigger ids (latched in
/// `field_208`); in state 5, 0x400C0005 while bit 2 of `field_58` is set.
/// Returns 0 otherwise.
s32 func_actor_223600_8014B464(Actor223600Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_208 == v) {
                goto same;
            }
            arg0->field_208 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_208 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_208 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B540);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B840);

INCLUDE_RODATA("actors/nonmatchings/actor_223600/actor_223600", D_actor_223600_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014BBF4);

/// The three state handlers the tick below picks between by the work block's
/// state word, copied onto the stack before the call. The copy is a three-word
/// block move out of the unit's `.rodata`, which is why the table is a rodata
/// object rather than a local initialiser.
const GpEnemyTaskFuncTable3 D_actor_223600_80149E4C = {
    {
        ActorsShared80134178,
        func_actor_223600_8014B840,
        func_actor_223600_8014BBF4,
    },
};

/// Per-frame tick of the park/unpark machine `ActorsShared80135df4` dispatches
/// to. The game mode word selects a one-shot arm first: mode 0 clears the
/// model's `field_C` when the work block is parked and then falls through to
/// the shared body, mode 1 does the same and returns, and mode 2 forces
/// `field_C` to 0x80 and returns. The shared body records the state change in
/// `field_4` and the dispatched state in `field_2`, runs the state handler,
/// turns the animation latch `func_actor_223600_8014B464` raises into a
/// `SndEvt_EnqueueType6` cue -- the work id from the enemy's `field_8` in its
/// bits 8-11, with the model's pan and depth -- and finally re-parks the model
/// through `func_800D7A9C` while `field_20C` is set, latching it once the
/// non-resident mode or an empty coordinate arrives.
void func_actor_223600_8014CA00(GpEnemy* enemy, Task* task)
{
    Actor223600Work*      work;
    GpEnemyTaskFuncTable3 fns;
    s32                   reaction;
    s32                   cue;
    s32                   pan;

    work = (Actor223600Work*)task->idMap;
    fns  = D_actor_223600_80149E4C;

    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0) {
                ((TmdObject*)task->extra)->field_C = 0;
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                ((TmdObject*)task->extra)->field_C = 0;
            }
            return;
        case 2:
            ((TmdObject*)task->extra)->field_C = 0x80;
            return;
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    fns.funcs[work->field_0](enemy, task);

    reaction = func_actor_223600_8014B464(work);
    if (reaction != 0) {
        cue = reaction | (((u16)enemy->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
        SndEvt_EnqueueType6(
            cue, pan,
            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
    }
    if (work->field_20C != 0) {
        func_800D7A9C((TmdObject*)task->extra,
                      (VECTOR*)((TmdObject*)task->extra)->field_8->workm.t, 0, 3);
    }
    if (Game_Session->field_4D != 0) {
        ((TmdObject*)task->extra)->field_8->flg = 0;
    }
    if (((TmdObject*)task->extra)->field_8->flg == 0) {
        work->field_20C = 1;
        return;
    }
    work->field_20C = 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_223600/actor_223600", ActorsShared80135df4Table);

/// Message handler (id 0x7D5 in `D_actor_223600_80150B28`). Drives the model's
/// `field_C` flag word and the work block's state word from `arg2`: 0 sets 0x80
/// and rewrites the buffers, 1 clears it and rewrites the buffers, 2 sets bit
/// 2, and 3 clears then sets bit 2. Only case 1 keeps `arg2` as the state.
s32 func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj  = task->extra;
    Actor223600Work* work = (Actor223600Work*)task->idMap;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = arg2;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

/// Message handler (id 0x7DB in `D_actor_223600_80150B28`). Copies the first
/// three bytes of the event packet into the work block, then, for command word
/// 0x302, drives the work block's state word from the packet's sub-command: 1
/// selects 2, 2 and 9 select 0, and 0 is a no-op.
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event)
{
    Actor223600Work* work;

    work            = (Actor223600Work*)task->idMap;
    work->field_180 = event->bytes[0];
    work->field_181 = event->bytes[1];
    work->field_182 = event->bytes[2];
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                work->field_0 = 2;
                break;
            case 2:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}
