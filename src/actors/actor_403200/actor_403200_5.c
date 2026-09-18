#include "common.h"
#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_actor_403200_80141C5A;

extern Task* D_actor_403200_8015F8F0;

void func_actor_403200_801410F0(s8 arg0)
{
    ((Actor403200Work*)D_actor_403200_8015F8F0->work)->field_EAC = arg0;
}

void func_actor_403200_80141108(s16 arg0)
{
    D_actor_403200_80141C5A = arg0;
}

s16 func_actor_403200_80141114(void)
{
    return D_actor_403200_80141C5A;
}

s32 func_actor_403200_80141124(Actor403200Obj* arg0, s16 arg1)
{
    switch (arg1) {
        case 0:
            return 0x13;
        case 1:
            return 7;
        case 2:
            return 0x25;
    }
    return 1;
}

s16 func_actor_403200_80141180(Actor403200Obj* arg0, s16 arg1)
{
    return func_actor_403200_801344C4(arg0, arg1);
}

/// Returns 0x25 for the current view, or 0x1E when the slot-3 model's X
/// translation is at or above a threshold that depends on the view index:
/// 0x3A98 for view 0x1E, 0x3E80 otherwise.
s32 func_actor_403200_801411A8(void)
{
    Task* task;
    s32   flag;
    s32   value;
    s32   view;

    view = Gp_GetViewIndex() & 0xFF;
    task = (Task*)Game_GetPtrSlot(3);
    if (view == 0x1E) {
        flag  = ((TmdObject*)task->extra)->coords->coord.t[0];
        flag  = flag < 0x3A98;
        value = 0x25;
    } else {
        flag  = ((TmdObject*)task->extra)->coords->coord.t[0];
        flag  = flag < 0x3E80;
        value = 0x25;
    }
    if (flag == 0) {
        value = 0x1E;
    }
    return value;
}

void func_actor_403200_8014122C(void)
{
}

void func_actor_403200_80141234(void)
{
}

/// The state handler `D_actor_403200_80132154` lists for state 8. Re-arms the
/// sub-state counter if the dispatcher saw a state change this tick, runs the
/// per-frame body, and on the tick the counter reaches 8 tells the player's
/// task (message 0x13F4) and plays the actor's cue.
void func_actor_403200_8014123C(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_6 = 0;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_6 == 8) {
        Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 0, 0);
        SndEvt_EnqueueType7(((enemy->field_8 >> 12) << 8) | 0x4020000A, 1);
    }
}
