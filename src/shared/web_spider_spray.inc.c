#include "main/random.h"

/* Part of the web spider library; see web_spider.h. */

/// Behaviour state 4: plays the spray sound at frame 0x28 and spawns a puff
/// projectile on each of frames 0x2B-0x31, counting them in field_3AC. It
/// returns to the idle-turn state (3) once the animation has run past the
/// spray's tail length, with a random delay taken from the placement-row table.
void spiderSprayState(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              sound;
    s32              pan;
    u32              random;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    work->field_398 = 0;
    work->field_3A6 = 0;
    if ((s16)work->field_396 == 0x28) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0003;
        pan   = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)gpGetObjDepth(coord));
    }
    if ((u32)(work->field_396 - 0x2B) < 7U) {
        Gp_SpawnEnemyFromTable(work->field_36C, 1, 0, arg0->spawnArg2.pointer);
        work->field_3AC = (u16)(work->field_3AC + 1);
    }
    if ((s16)work->field_396 >= (gSpiderSprayTail + 0x3C)) {
        work->field_39A = 3;
        work->field_39C = 0;
        work->field_392 = 1;
        random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->field_39E = gSpiderIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
        gRandomLcgState = random;
    }
}
