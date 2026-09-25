#include "common.h"

#include "main/session.h"
#include "main/sound.h"

#include "gameplay/3A34.h"

#include "actors/actor_403900.h"

/// Base id of the actor's vocal cue: the `GpEnemy` work id's high nibble is
/// OR'd in as bits 8-11 of the cue id.
extern s32 D_actor_403900_80138478;

/// Runs the one-shot vocal cue armed by `field_718`. While the flag is set the
/// frame counter `field_71A` runs up: at 0x14 the cue is queued, panned and
/// depth-attenuated from the display object, and from 0x5F on the player's
/// slot is asked for message 0x3ED - a refusal sends 0x3F1 instead and clears
/// the flag.
void func_actor_403900_801380DC(Actor403900* arg0)
{
    Actor403900Work* work;
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
            sound = D_actor_403900_80138478 | ((arg0->field_20->placeKey >> 12) << 8);
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
