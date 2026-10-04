#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY`: plays the spray sound at frame 0x28
/// and spawns a puff projectile on each of frames 0x2B-0x31, counting them in
/// `puffCount`. It returns to `MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM` once the
/// animation has run past the spray's tail length, with a random delay taken
/// from the placement-row table.
void maggotCaterpillarSprayState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    sound;
    s32                    pan;
    u32                    random;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->forwardSpeed = 0;
    work->turnRate     = 0;
    if (work->animFrame == 0x28) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0003;
        pan   = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if ((work->animFrame >= 0x2B) && (work->animFrame < 0x32)) {
        Gp_SpawnEnemyFromTable(work->taskTable, 1, 0, arg0->spawnArg2.pointer);
        work->puffCount++;
    }
    if (work->animFrame >= (gMaggotCaterpillarSprayTail + 0x3C)) {
        work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
        work->step         = 0;
        work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
        random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
        gRandomLcgState    = random;
    }
}
