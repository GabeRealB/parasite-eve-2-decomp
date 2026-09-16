#include "common.h"

#include "actors/actors_shared_801390d8.h"

/// Sub-state handler built around the halfword countdown at 0xB8C.
///
/// The first frame arms it: `field_BA4` takes 0x10, or 0xF while the flag at
/// 0xBAE is clear, `field_BAF` is set to 2 and the countdown is zeroed, and the
/// run-once latch at 0xBA8 is stepped. Every later frame moves the countdown up
/// by one and, on the frame it reaches 0x28 - 0x3C while 0xBAE is set - clears
/// `field_BAF` again. The frame block's scratch byte at 0x64 takes 3 either
/// way, and the trigger at 0xBA9 ends the sub-state by dropping the enemy out
/// of its link-node slot, latching `field_BA6` and switching `state` to 0xF.
///
/// Both halves store into the field from inside each arm rather than through a
/// shared local: a local's first definition would land before the branch on
/// 0xBAE, and jump.c's arm collapse hoists one arm's constant in front of that
/// branch, which then keeps the flag and the value in separate registers.
void ActorsShared801390d8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 count;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0xF;
        } else {
            work->field_BA4 = 0x10;
        }
        work->field_BAF = 2;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        return;
    }
    count           = (u16)work->field_B8C + 1;
    work->field_B8C = count;
    if (work->field_BAE == 0) {
        if ((s16)count == 0x28) {
            work->field_BAF = 0;
        }
    } else {
        if ((s16)count == 0x3C) {
            work->field_BAF = 0;
        }
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        enemy->node.field_4 = 0;
        work->field_BA6     = 1;
        work->field_BAB     = 0;
        work->state         = 0xF;
        work->field_BA8     = 0;
    }
}
