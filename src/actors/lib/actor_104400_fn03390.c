#include "common.h"

#include "actors/actor_104400.h"
#include "main/task.h"

void Actor04400_Fn06374(Task* arg0, s32 arg1);

/// Moves the state machine to `state` at sub-state 0, reloading the work
/// block through the task as the original does.
static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor104400Work* w = (Actor104400Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Inlined copy of `Actor04400_Fn063E4`: while `field_41E` is 1, consumes the
/// request in `field_448` (1..5 jump to states 6, 7, 8, 7, 9) and returns 1;
/// otherwise returns 0.
static __inline__ s32 take_request(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0:
                set_state(arg0, 6);
                break;
            case 1:
                set_state(arg0, 7);
                break;
            case 2:
                set_state(arg0, 8);
                break;
            case 3:
                set_state(arg0, 7);
                break;
            case 4:
                set_state(arg0, 9);
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

static __inline__ s32 is_hit(Task* arg0)
{
    Actor104400Work* w = (Actor104400Work*)arg0->work;

    if ((w->flags_EC.half & 1) || (w->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// State handler: with `field_44F` 1, a pending request 1 while `field_41E`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `field_44F` clear, a hit
/// calls `Actor04400_Fn06374` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `field_44F`
/// register; writing `== work->field_44F` reloads the byte instead.
void Actor04400_Fn03390(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_44F == 1) {
        if (work->field_41E != 0 && work->field_448 == 1) {
            work->field_41C = 0x20;
            work->field_418 = 0xB;
            work->field_414 = 2;
            return;
        }
        if (take_request(arg0) == 0 && is_hit(arg0)) {
            set_state(arg0, 3);
        }
    } else if (is_hit(arg0)) {
        Actor04400_Fn06374(arg0, 1);
        set_state(arg0, 5);
    }
}
