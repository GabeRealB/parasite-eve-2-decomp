#include "common.h"
#include "actors/actor_105500.h"
#include "main/sound.h"
#include "gameplay/3A34.h"

void ActorsSharedFn020d4(Actor105500* arg0)
{
    Actor105500Work* work;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    u16              timer;
    u16              effectTimer;
    u16              countdown;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    timer           = work->field_3B2 + 1;
    work->field_3B2 = timer;
    if ((s16)timer >= 0x50) {
        work->field_3B2 = 0U;
    }
    effectTimer     = work->field_3B4 + 1;
    work->field_3B4 = effectTimer;
    if ((s16)effectTimer == 0xC) {
        work->field_3B4 = 0U;
        if (work->field_3B6 == 0) {
            func_800FDB18(3, arg0->field_2C->field_8 + 3, NULL, &work->field_354);
            work->field_3B6 = 1;
        } else {
            func_800FDB18(3, arg0->field_2C->field_8 + 5, NULL, &work->field_354);
            work->field_3B6 = 0;
        }
    }
    countdown       = work->field_3BE - 1;
    work->field_3BE = countdown;
    if ((s16)countdown <= 0) {
        work->field_3BE = 0x24U;
        sound           = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0005;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth(coord));
    }
}
