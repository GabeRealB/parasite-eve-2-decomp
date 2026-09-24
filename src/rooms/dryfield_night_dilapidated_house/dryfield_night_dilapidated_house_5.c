#include "common.h"

#include "main/task.h"

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
void func_dryfield_night_dilapidated_house_8017DA08(Task* task)
{
    char pad[0x10];
}
