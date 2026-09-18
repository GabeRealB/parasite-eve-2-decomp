#include "common.h"

#include "actors/actors_shared_80133474.h"

#include "main/tmd.h"

/// Applies the visibility opcode: hides or restores the actor's own model and
/// the model of the task its spawn routine paired it with, then optionally
/// ORs the `Tmd_Create` visibility bit into both.
s32 ActorsShared80133474(Task* task, s32 arg1, s32 flags)
{
    ActorsShared80133474Work* work;
    TmdObject*                self;
    TmdObject*                other;

    self = (TmdObject*)task->extra;
    work = (ActorsShared80133474Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->field_4F4->extra;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->field_C  = 0;
        other->field_C = 0;
    } else {
        self->field_C  = 0x80;
        other->field_C = 0x80;
    }
    if (flags & 2) {
        self->field_C  |= 4;
        other->field_C |= 4;
    }
    return 0;
}
