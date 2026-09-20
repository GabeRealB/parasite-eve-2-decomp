#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_402200.h"

/// Base id of the actor's vocal cues: the `GpEnemy` work id's high nibble
/// selects one of the four adjacent words here, picked up as bits 8-11 of the
/// cue id.
extern s32 D_actor_402200_80138474;

void func_actor_402200_801380D8(Actor402200* arg0)
{
    Actor402200Work* work;
    s16              timer;
    s32              sound;
    s32              pan;
    Task*            slot;
    GsCOORDINATE2*   coord;

    work  = arg0->field_1C;
    coord = (GsCOORDINATE2*)arg0->field_2C->field_8;
    slot  = gameGetPtrSlot(3);
    if (work->field_718 != 0) {
        if (work->field_71A == 0x14) {
            sound = D_actor_402200_80138474 | ((arg0->field_20->placeKey >> 12) << 8);
            pan   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        }
        timer           = (u16)work->field_71A + 1;
        work->field_71A = timer;
        if ((timer >= 0x5F) && (Gp_DispatchMsg(slot, 0x3ED, 0, 0) == 0)) {
            Gp_DispatchMsg(slot, 0x3F1, 0, 0);
            work->field_718 = 0;
        }
    }
}
