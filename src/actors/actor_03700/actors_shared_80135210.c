#include "common.h"

#include "actors/actor_103700.h"

/// `func_800B4114` is deliberately not declared by `gameplay/1BC.h`; its
/// definition takes `arg2` as `u16`, which would add a zero-extension no
/// caller has. See the note in that header.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Drives the five animation slots from the requested animation `field_248`.
/// When the request differs from the one playing (`field_24A`), it is
/// latched, the frame counter `field_24C` restarts and every slot is pointed
/// at it with a blend of 4; otherwise the counter ticks and each slot advances.
void Actor03700_Fn033F0(Task* task)
{
    Actor103700Work* work;
    s32              i;

    work = (Actor103700Work*)task->work;
    i    = 1;
    if (work->field_248 != work->field_24A) {
        work->field_24A = (u16)work->field_248;
        work->field_24C = 0;
        do {
            func_800B4114(work, i, work->field_248, 0, 4);
            i++;
        } while (i < 6);
        return;
    }
    TOUCH_REG(i);
    work->field_24C += i;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 6);
}
