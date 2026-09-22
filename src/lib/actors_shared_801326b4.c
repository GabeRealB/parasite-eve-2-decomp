#include "common.h"

#include "actors/actors_shared_801326b4.h"

#include "main/tmd.h"

/// Applies `arg2` to the overlay's published task extra: bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden); bit 1 ORs in 0x4.
s32 ActorsShared801326b4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)ActorsShared801326b4Task->extra;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}
