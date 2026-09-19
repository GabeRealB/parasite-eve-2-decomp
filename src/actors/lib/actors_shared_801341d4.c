#include "common.h"

#include "actors/actors_shared_801341d4.h"
#include "main/sound.h"

#include "gameplay/3A34.h"

void ActorsShared801341d4(ActorShared801341d4* arg0)
{
    GsCOORDINATE2*           object;
    s16                      scale;
    s16                      ramp;
    s32                      soundId;
    s32                      volume;
    s8                       depth;
    u16                      counter;
    ActorShared801341d4Work* work;

    work            = arg0->field_1C;
    object          = (GsCOORDINATE2*)arg0->field_2C->field_8;
    counter         = work->field_14A + 1;
    work->field_14A = counter;
    if ((s16)counter >= 0x19) {
        work->field_14A = 0;
        scale           = work->field_12A;
        soundId         = (((u16)arg0->field_20->placeKey >> 0xC) << 8) | 0x40180001;
        if (scale >= 0x1D01) {
            ramp = 0x1700;
        } else {
            ramp = (u16)work->field_12A - 0x600;
            if (scale < 0x600) {
                ramp = 0;
            }
        }
        volume = (ramp * 0x32) / 5888 + 0x32;
        depth  = 0x7F - (((0x7F - Gp_GetObjDepth(object)) * (s16)volume) / 100);
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(object), depth);
    }
}
