#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300_4", func_actor_146300_8013291C);

/// Play-animation message handler: adopts `preset`'s animation id when it is
/// one of the first 0x11, latching the reset mode and the reset argument the
/// reseed forwards, then hands the published task to the per-frame update. Ids
/// past the range are rejected with -1 and leave the work block untouched.
s32 func_actor_146300_8013299C(Task* task, s32 arg1, Actor146300AnimPreset* preset)
{
    if (preset->field_4 < 0x11) {
        ActorsShared80131f9cWork->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            ActorsShared80131f9cWork->field_4B4 = 1;
            D_actor_146300_8014279C             = preset->field_C;
        } else {
            ActorsShared80131f9cWork->field_4B4 = 2;
        }
        ActorsShared80131f9cWork->field_4BA = 0;
        func_actor_146300_801327CC(D_actor_146300_8014282C);
        return 0;
    }
    return -1;
}

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300_4", func_actor_146300_80132A2C);
