#include "common.h"

#include "actors/actors_shared_80138d58.h"
#include "main/sound.h"

/// First frame of the sub-state arms motion 5 and the 0x64-frame countdown at
/// 0xB8C, then bumps the run-once latch at 0xBA8. Every later frame steps that
/// countdown, and on the frame it reaches zero cues the 0x400B0004 event - the
/// actor's id byte at 0xB88 in bits 8..15, the variant byte at 0xBB8 in bit 22,
/// pan and depth from the frame block - through `SndEvt_EnqueueType6`. The
/// scratch byte at 0x64 then takes 0xC while bit 0 of `D_80070F70` is set and 8
/// otherwise, and the trigger at 0xBA9 ends the sub-state by clearing both the
/// state and the latch.
void ActorsShared80138d58(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    if (work->field_BA8 == 0) {
        work->field_BA4 = 5;
        work->field_B8C = 0x64;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        return;
    }
    if (work->field_B8C != 0) {
        work->field_B8C--;
        if (work->field_B8C == 0) {
            SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0004), arg->pan, arg->depth);
        }
    }
    if (D_80070F70 & 1) {
        arg->field_64 = 0xC;
    } else {
        arg->field_64 = 8;
    }
    if (work->field_BA9 == 1) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}
