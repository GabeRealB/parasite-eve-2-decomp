#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

#include "actors/actor_342400.h"

/// Moves the task to `state` and rewinds the state machine to state 0.
static __inline__ void enter_state(Task* arg0, s32 state)
{
    Actor342400Work* w = (Actor342400Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Jumps the state machine to `state`, sub-state 0.
static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor342400Work* w = (Actor342400Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Once bit 7 of `Gp_StateF0.field_1F` is set, puts the task in state 3 with
/// the state machine at state 5 and returns 1; otherwise returns 0.
s16 func_actor_342400_8016945C(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        enter_state(arg0, 3);
        set_state(arg0, 5);
        return 1;
    }
    return 0;
}
