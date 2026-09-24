#include "common.h"

/// Idle state of the room's entry task: does nothing. The unused 0x10-byte
/// local reproduces the stack frame the retail code reserves.
void func_neo_ark_garden_8017EA34(void)
{
    char pad[0x10];
}
