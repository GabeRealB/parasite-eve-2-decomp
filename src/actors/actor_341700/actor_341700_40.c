#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

#include "actors/actor_341700.h"

/// Moves the task to `state` and rewinds the state machine to state 0.
static __inline__ void enter_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Jumps the state machine to `state`, sub-state 0.
static __inline__ void set_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Holds for `field_446` frames, then moves to state 2. Over the last 0x30
/// frames the head yaw `field_424` eases back to zero; before that, while a
/// player actor is within 0xDAC and roughly ahead, it turns toward it and
/// after 16 such frames arms `Gp_StateF0` and moves to state 3, and
/// otherwise it sways between two fixed yaws by bit 6 of `field_442`.
void func_actor_341700_8016583C(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    Actor341700Work* state;
    Actor341700Work* state2;
    s32              angle;
    s32              cur;
    s32              aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (Actor341700Work*)arg0->work;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (Actor341700Work*)arg0->work;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}

/// Side-steps to the right of the heading at a speed scaled by `field_41C`
/// on frames 0x1D..0x29; once the hit flags are set, moves the task and the
/// state machine to state 3.
void func_actor_341700_80165984(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                          = 0x1E;
        angle                                          = work->field_7A + 0x400;
        speed                                          = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        enter_state(arg0, 3);
        set_state(arg0, 3);
    }
}

/// The same side-step as `func_actor_341700_80165984`, but once the hit
/// flags are set it requests animation 3 and advances the sub-state instead.
void func_actor_341700_80165AF0(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                          = 0x1E;
        angle                                          = work->field_7A + 0x400;
        speed                                          = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work2 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438  = 0;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 3;
        work2->field_414 = 1;
        work->field_412  = 0;
        work->field_422++;
    }
}

/// Unless `func_actor_341700_80168178` takes over, side-steps to the left on
/// frames 0x17..0x23 and, once the hit flags are set, returns the state
/// machine to state 0.
void func_actor_341700_80165C70(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor341700Work*)arg0->work;
    if ((func_actor_341700_80168178(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                          = -0x1E;
            angle                                          = work->field_7A + 0x400;
            speed                                          = (((Actor341700Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
            ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->coords->flg         = 0;
        }
        work2 = (Actor341700Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_438 = 0;
            next            = (Actor341700Work*)arg0->work;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}
