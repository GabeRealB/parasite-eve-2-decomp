#include "common.h"

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
void func_dryfield_night_saloon_g_r_8017E040(void)
{
    char pad[0x10];
}
