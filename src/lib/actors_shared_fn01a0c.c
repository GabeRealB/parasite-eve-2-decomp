#include "common.h"

#include "actors/actor_105500.h"
#include "gameplay/3A34.h"
#include "main/sound.h"

void Actor05500_Fn01A0C(Actor105500* arg0)
{
    Actor105500Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              sound;
    s32              pan;
    u32              random;

    work  = arg0->field_1C;
    state = work->field_39C;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_392 = 0xB;
            work->field_394 = 1;
            work->field_39C = 1;
            work->field_398 = 0;
            work->field_3A6 = 0;
            sound           = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0004;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            return;
        case 1:
            if ((s16)work->field_396 >= 0x15) {
                if (work->field_3D2 == state) {
                    work->field_39A = 7;
                    work->field_39C = 0;
                    return;
                }
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = (random >> 0x10) & 0xF;
            } else {
                return;
            }
            break;
    }
}
