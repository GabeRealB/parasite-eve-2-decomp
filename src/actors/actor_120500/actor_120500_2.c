#include "common.h"

#include "actors/actor_120500.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_120500_801328C0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4B8 = arg0;
    work->field_4BA = 0;
}

void func_actor_120500_801328E0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120500_80132900(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

/// Brings the actor up for its scene: plays sound cue `0x521E0007`, broadcasts
/// message 0x7D5 -- the visibility control the display task handles -- to the
/// actor itself with payload 2, and clears the three request/phase pairs at
/// 0x4B8, 0x4C0 and 0x4C8 the three setters above arm. The task at `field_4B4`
/// then gets animation set `D_actor_120500_8013807C` on slot 2 (message 0x3F4
/// with `field_8` and `field_C` zero and `field_10` set), message 0x3F3 with
/// payload 1, and the placement record `D_actor_120500_801380A8` as message
/// 0x3E9, with the override vector cleared in between. Both the work pointer
/// and the target task are re-read after every call: the model state those
/// messages install is what the next send is addressed through.
void func_actor_120500_80132920(void)
{
    Actor120500*     actor;
    Actor120500Work* work;
    Actor120500Work* animWork;
    GpAnimArg        msg;

    actor = D_actor_120500_80138454;
    work  = actor->field_1C;
    SndEvt_EnqueueType7(0x521E0007, 0xA);
    Gp_DispatchMsg((Task*)actor, 0x7D5, 2, 0);
    work->field_4B8 = 0;
    work->field_4C0 = 0;
    work->field_4C8 = 0;
    animWork        = actor->field_1C;
    if (animWork->field_4B4 != NULL) {
        msg.field_0  = D_actor_120500_8013807C;
        msg.field_4  = 2;
        msg.field_8  = 0;
        msg.field_C  = 0;
        msg.field_10 = 1;
        Gp_DispatchMsg(animWork->field_4B4, 0x3F4, (s32)&msg, 0);
    }
    work = actor->field_1C;
    Gp_SetOverrideVec(NULL);
    Gp_DispatchMsg(work->field_4B4, 0x3F3, 1, 0);
    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_120500_801380A8, 0);
}

/// Draw/alloc bits of the actor's `TmdObject` extra (`field_C`): mode 0 hides
/// the model (set 0x80) and leaves the alloc bit alone, mode 1 shows it and
/// allows alloc (clear 0x80 and 0x4), mode 2 hides it and skips alloc (set
/// 0x80 and 0x4). Mode 2 is written as a set of 0x4 that falls into mode 0,
/// not as one `| 0x84`, and that fallthrough is what its branch layout is
/// built from. Same handler shape as `ActorsShared80163224` / `Room_Util19`,
/// but this copy is not shared: it is reached from this overlay's trailing
/// dispatch table, which is the only reference to it.
void func_actor_120500_80132A04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 2:
            extra->field_C = extra->field_C | 4;
            /* fallthrough */
        case 0:
            extra->field_C = extra->field_C | 0x80;
            return;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            return;
    }
}
