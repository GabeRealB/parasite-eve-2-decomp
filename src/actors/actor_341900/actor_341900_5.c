#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_341900.h"

/// Message 0x7D5 handler of both of the overlay's message tables: sets the
/// draw bits of the task's `TmdObject` from the mode in `arg2`. Mode 0 sets
/// 0x80 and clears 0x4, mode 1 clears both, mode 2 sets both.
void func_actor_341900_80163224(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)arg0->extra;
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
