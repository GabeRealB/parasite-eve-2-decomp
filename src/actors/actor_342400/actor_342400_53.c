#include "common.h"

#include "main/task.h"

#include "actors/actor_342400.h"

extern u32 Gp_LcgState;

/// Requests animation 1, draws a 0x60..0x9F frame hold into `field_446`,
/// clears the frame counter and advances the sub-state.
void func_actor_342400_8016B0A0(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->work;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    /* Rolling the LCG through the global rather than an m2c temporary is what
     * hoists its `lw` above the field stores; see DECOMPILATION_LEARNINGS.md. */
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_446 = ((Gp_LcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}

/// Once the hold in `field_446` runs out, picks state 4 or 1 at random.
/// Before that, a player actor within 0xDAC moves the state machine to
/// state 3 and one within 0x1388 advances the sub-state.
void func_actor_342400_8016B104(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;
    s16              dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            Actor342400Work* w = (Actor342400Work*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            Actor342400Work* w = (Actor342400Work*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        Actor342400Work* next = (Actor342400Work*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}

/// Once the hit flags are set, moves the state machine to state 1.
void func_actor_342400_8016B1C8(Task* arg0)
{
    Actor342400Work* work;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor342400Work*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}

/// Once the hit flags are set, requests animation 0xD and advances the
/// sub-state.
void func_actor_342400_8016B21C(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}

/// Once the hit flags are set, requests animation 0xE, clears the frame and
/// turn counters, draws a 0xB0..0xEF frame hold into `field_446` and
/// advances the sub-state.
void func_actor_342400_8016B294(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 0xE;
        work2->field_414 = 1;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        work->field_412  = 0;
        work->field_42C  = 0;
        work->field_446  = ((Gp_LcgState >> 16) & 0x3F) + 0xB0;
        work->field_422++;
    }
}
