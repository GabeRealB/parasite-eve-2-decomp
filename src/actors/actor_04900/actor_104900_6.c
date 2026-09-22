#include "common.h"

#include "actors/actors_shared_80138efc.h"
#include "main/sound.h"

void Actor04900_Fn05678(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);

/// Countdown handler built around the halfword at 0xB8C, the one after the
/// 0x801390D8 body in the family's state table.
///
/// The first frame arms the motion pair: `field_BA4` takes 0xA, or 0xD while
/// the flag at 0xBAE is set, `field_BAF` takes 1 and the countdown is zeroed,
/// with the run-once latch at 0xBA8 stepped in both cases. Every later frame
/// moves the countdown up by one and, on the frame it reaches 0xD, cues the
/// 0x400B0003 event - the actor's id byte at 0xB88 in bits 8..15 and the
/// variant at 0xBB8 in bit 22, pan and depth from the frame block - then parks
/// the countdown at -0x7FFF so it fires only once. The frame block's scratch
/// byte at 0x64 takes 3 either way, and the trigger at 0xBA9 ends the
/// sub-state: while `field_B92` still counts it keeps the state on the 0x17
/// motion with the 0x10 pair when the enemy is not carrying flag 0x2 in
/// `field_4C`, and stages the 0x14 motion through `field_BA6` when it is; once
/// that count has run out it hands the frame to the shared routine at
/// 0x80137498 on the 0x18 motion instead.
void Actor04900_Fn07148(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 time;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0xA;
        } else {
            work->field_BA4 = 0xD;
        }
        work->field_BAF = 1;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time >= 0xD) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0003), arg->pan, arg->depth);
        work->field_B8C = -0x7FFF;
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->reactionFlags & 2)) {
                work->field_BAB = 0x10;
                work->state     = 0x17;
                work->field_BA8 = 0;
                work->field_BAF = 2;
                return;
            }
            work->field_BA6 = 2;
            work->field_BAB = 5;
            work->state     = 0x14;
            work->field_BA8 = 0;
            return;
        }
        work->state     = 0x18;
        work->field_BA8 = 0;
        Actor04900_Fn05678(enemy, task, work, arg);
    }
}
