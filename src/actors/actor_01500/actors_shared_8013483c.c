#include "common.h"

#include "actors/actor_101500.h"
#include "gameplay/3A34.h"
#include "main/sound.h"

/// Voice tick: while `field_34C` holds a sound id, plays it every third
/// animation frame, tagged with the placement number, and once `field_380`
/// runs out switches to the 0x400F0003 cue.
void Actor01500_Fn02A1C(Actor101500* arg0)
{
    s16              timer;
    s32              soundId;
    s32              objectSoundId;
    s32              pan;
    GsCOORDINATE2*   object;
    Actor101500Work* work;

    work          = arg0->field_1C;
    objectSoundId = work->field_34C;
    object        = arg0->field_2C->coords;
    if (objectSoundId != 0) {
        if ((s16)((s16)work->field_356 % 3) == 1) {
            soundId = objectSoundId | ((arg0->field_20->placeKey >> 0xC) << 8);
            pan     = (s8)Gp_GetObjPan(object);
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
        }
        if (work->field_380 > 0) {
            timer           = (u16)work->field_380 - 1;
            work->field_380 = timer;
            if ((timer << 0x10) <= 0) {
                work->field_34C = 0x400F0003;
                work->field_380 = 0;
            }
        }
    }
}
