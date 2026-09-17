#include "common.h"

#include "actors/actor_450800.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_450800_80132AE0(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->idMap;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->field_4B8, 0, work->field_4FC);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

void func_actor_450800_80132448(Task* task);

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths `func_actor_450800_80132448`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_450800_80132B44(Task* task, s32 arg1, Actor450800AnimArgs* args)
{
    Actor450800Work* work;

    work = (Actor450800Work*)task->idMap;
    if (args->animId >= 0x1F) {
        return -1;
    }

    work->field_4B8 = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state     = 1;
        work->field_4FC = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_450800_80132448(task);
    return 0;
}

/// Message handler 0x7D5 of `D_actor_450800_8014AC58`: sets the visibility
/// flags on this actor's own model and on the three helper tasks' ones at once.
///
/// `arg2` bit 0 hides -- `field_C = 0` -- rather than shows (`0x80`), and bit 1
/// ORs 4 in, so the payload 0 `Gp_MsgSlot4Chain` sends for the first chain entry
/// shows all four. `Actor450800Work::field_500` overrides the last of them:
/// while it is 0 the helper at `field_4F8` keeps the 0x84 appearance handler
/// 0x7DB's mode 2 gave it, instead of the flags just computed.
s32 func_actor_450800_80132BB0(Task* task, s32 arg1, s32 arg2)
{
    Actor450800Work* work;
    TmdObject*       self;
    TmdObject*       first;
    TmdObject*       second;
    TmdObject*       third;

    work   = (Actor450800Work*)task->idMap;
    self   = (TmdObject*)task->extra;
    first  = (TmdObject*)work->field_4F0->extra;
    second = (TmdObject*)work->field_4F4->extra;
    third  = (TmdObject*)work->field_4F8->extra;

    if (arg2 & 1) {
        self->field_C   = 0;
        first->field_C  = 0;
        second->field_C = 0;
        third->field_C  = 0;
    } else {
        self->field_C   = 0x80;
        first->field_C  = 0x80;
        second->field_C = 0x80;
        third->field_C  = 0x80;
    }
    if (arg2 & 2) {
        self->field_C   |= 4;
        first->field_C  |= 4;
        second->field_C |= 4;
        third->field_C  |= 4;
    }
    if (work->field_500 == 0) {
        third->field_C = 0x84;
    }
    return 0;
}
