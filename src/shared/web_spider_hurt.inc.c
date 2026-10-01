#include "main/random.h"

/* Part of the web spider library; see web_spider.h. */

/// Behaviour state 6, entered when a hit does damage. On entry it starts
/// animation 0xB, stops the forward and turn steps and plays sound
/// 0x401A0004 with the top nibble of the context's `field_8` in bits 8-11,
/// panned to the actor. Once the
/// animation has run 0x15 frames it goes to state 7 when `field_3D2` is 1,
/// otherwise to state 3 with animation 1 and a random 0..15 in `field_39E`.
void spiderHurtState(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              sound;
    s32              pan;
    u32              random;

    work  = arg0->work;
    state = work->field_39C;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_392 = 0xB;
            work->field_394 = 1;
            work->field_39C = 1;
            work->field_398 = 0;
            work->field_3A6 = 0;
            sound           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0004;
            pan             = (s8)worldCoordGetOriginAudioPan(coord);
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
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                work->field_39E = (random >> 0x10) & 0xF;
            } else {
                return;
            }
            break;
    }
}
