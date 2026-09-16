#include "common.h"

#include "actors/actor_120500.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/task.h"

/// Animation-set table this overlay hands the task at `field_4B4` as message
/// 0x3F4's `GpAnimArg::field_0`: three sets, the same shape
/// `D_actor_444000_8014430C` has.
extern GpAnimSet* D_actor_120500_8013807C[];

/// Placement record message 0x3E9 hands that same task, taken as a raw
/// address the way `D_actor_120300_80140ACC` is.
extern s32 D_actor_120500_801380A8;

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

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500_2", func_actor_120500_80132A04);
