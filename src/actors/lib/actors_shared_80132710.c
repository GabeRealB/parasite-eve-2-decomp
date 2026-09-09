#include "common.h"

#include "actors/actors_shared_80132710.h"

#include "main/tmd.h"

/// `flags` bit 0 hides both models (`TmdObject::field_C` = 0) and its absence
/// restores the default 0x80; bit 1 additionally ORs in 0x4, the same bit
/// `Tmd_Create` sets for its own `flags & 1`.
s32 ActorsShared80132710(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((ActorsShared80132710Work*)task->idMap)->field_4B8->extra;

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
