#include "common.h"

#include "actors/actor_223600.h"

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014A170);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014A4B8);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014AA04);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014ABA8);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B2F4);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B464);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B540);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B840);

INCLUDE_RODATA("actors/nonmatchings/actor_223600/actor_223600", D_actor_223600_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014BBF4);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014CA00);

INCLUDE_RODATA("actors/nonmatchings/actor_223600/actor_223600", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014CC04);

/// Message handler (id 0x7DB in `D_actor_223600_80150B28`). Copies the first
/// three bytes of the event packet into the work block, then, for command word
/// 0x302, drives the work block's state word from the packet's sub-command: 1
/// selects 2, 2 and 9 select 0, and 0 is a no-op.
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event)
{
    Actor223600Work* work;

    work            = (Actor223600Work*)task->idMap;
    work->field_180 = event->bytes[0];
    work->field_181 = event->bytes[1];
    work->field_182 = event->bytes[2];
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                work->field_0 = 2;
                break;
            case 2:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}
