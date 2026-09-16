#include "common.h"

#include "actors/actor_210600.h"

#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_80149E30);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014A13C);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014A484);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014A9D0);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014AB74);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014B2C0);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014B434);

/// Display-object mode handler. `arg2` selects the mode: 0 hides the display
/// object by setting bit 0x80 of `TmdObject.field_C`, 1 clears `field_C` and so
/// shows it, 2 sets bit 0x4, and any other value clears the field and then sets
/// bit 0x4. Modes 0 and 1 reinstate the object's buffers through
/// `Tmd_AllocBuffers`; modes 0 and 2 arm the work block's 0x890 flag where the
/// other two clear it. `arg1` is unused; it exists because the dispatch passes
/// three arguments.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor210600Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor210600Work*)task->idMap;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_890 = 1;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_890 = 0;
            break;
        case 2:
            obj->field_C   |= 4;
            work->field_890 = 1;
            break;
        default:
            obj->field_C    = 4;
            work->field_890 = 0;
            break;
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_210600/actor_210600", D_actor_210600_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_210600/actor_210600", D_actor_210600_80149E24);
