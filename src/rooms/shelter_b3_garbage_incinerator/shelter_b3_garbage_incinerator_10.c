#include "common.h"

#include "rooms/room_common.h"

/// Sets how the task's model is treated from `arg2`: 0 hides it and leaves its
/// primitive buffer to be allocated on demand, 1 shows it with the same
/// allocation, 2 hides it and exempts it from that allocation.
void func_shelter_b3_garbage_incinerator_8017E690(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}
