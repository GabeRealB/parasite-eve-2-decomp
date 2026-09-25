#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Scene-script handler that sets the draw bits of the task's `TmdObject`:
/// mode 0 hides the model (0x80) and clears 0x4, mode 1 shows it and clears
/// 0x4, mode 2 hides it and sets 0x4.
void func_actor_121300_801341A8(Task* arg0, s32 arg1, s32 arg2)
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
