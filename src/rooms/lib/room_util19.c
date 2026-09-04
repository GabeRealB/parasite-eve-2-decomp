#include "common.h"

#include "rooms/room_common.h"

/// Writes the draw/alloc bits of the task's `TmdObject` extra (`field_C`):
/// mode 0 hides the model (set 0x80) and allows buffer alloc (clear 0x4);
/// mode 1 shows it and allows alloc; mode 2 hides it and skips alloc.
void Room_Util19(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            extra->field_C = (extra->field_C | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->field_C = extra->field_C & 0xFF7B;
            return;
        case 2:
            extra->field_C = extra->field_C | 0x84;
            return;
    }
}
