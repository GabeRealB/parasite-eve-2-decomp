#include "common.h"

#include "actors/actors_shared_8013483c.h"
#include "main/sound.h"

#include "gameplay/3A34.h"

void ActorsShared8013483c(ActorShared801342a4* arg0)
{
    s16                      timer;
    s32                      soundId;
    s32                      objectSoundId;
    s32                      pan;
    GpObj38*                 object;
    ActorShared801342a4Work* work;

    work          = arg0->field_1C;
    objectSoundId = work->field_34C;
    object        = arg0->field_2C->field_8;
    if (objectSoundId != 0) {
        if ((s16)(work->field_356 % 3) == 1) {
            soundId = objectSoundId | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
            pan     = (s8)Gp_GetObjPan(object);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
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
