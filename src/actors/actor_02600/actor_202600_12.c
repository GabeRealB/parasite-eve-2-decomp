#include "common.h"

#include "actors/actor_202600.h"
#include "gameplay/3A34.h"
#include "main/sound.h"

/// Behaviour state 6, entered when a hit does damage. On entry it starts
/// animation 0xB, stops the forward and turn steps and plays sound
/// 0x401A0004 with the top nibble of the context's `field_8` in bits 8-11,
/// panned to the actor. Once the
/// animation has run 0x15 frames it goes to state 7 when `field_3D2` is 1,
/// otherwise to state 3 with animation 1 and a random 0..15 in `field_39E`.
void Actor02600_Fn01A0C(Actor202600* arg0)
{
    Actor202600Work* work;
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
            sound           = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x401A0004;
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
