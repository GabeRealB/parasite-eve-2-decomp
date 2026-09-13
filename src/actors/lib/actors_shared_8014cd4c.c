#include "common.h"

#include "actors/actors_shared_8014cd4c.h"

#include "main/tmd.h"

/// `flags` bit 0 hides both models (`TmdObject::field_C` = 0) and its absence
/// restores the default 0x80; bit 1 additionally ORs in 0x4, the same bit
/// `Tmd_Create` sets for its own `flags & 1`.
///
/// The unused middle argument is load-bearing. It is what puts `flags` in
/// `$a2`, which frees `$a1` for the second model pointer; declaring the two
/// used arguments alone moves the flag down to `$a1`, pushes the pointer to
/// `$a2`, and the body stops matching.
s32 ActorsShared8014cd4c(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((ActorsShared8014cd4cWork*)task->idMap)->field_4F0->extra;

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
