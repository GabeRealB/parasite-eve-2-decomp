#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Kills the first enemy: its HP is cleared and a random draw (or a non-zero
/// `arg1`) picks the death. The violent one plays its sound, sets the 0x8000
/// bit of the last two bodies, spawns the 0x6009C and 0x60030 effects and the
/// death script, and asks for a final effect through `hasBurst`; the other
/// plays a second sound and moves the reaction state to 6. `variant` picks
/// the sound set either way.
void sucklercephKill(Task* arg0, u8 arg1)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              soundId;

    obj             = arg0->extra.tmd;
    enemy           = arg0->spawnArg2.pointer;
    work            = arg0->work;
    coord           = obj->coords;
    enemy->hp       = 0;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if (((gRandomLcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->variant != 0) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000B;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0003;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->blastBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 1, NULL);
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords, 0x300, &gSucklercephBurstFxOffset);
        Gp_SpawnScript18(gSucklercephBurstScriptA, gSucklercephBurstScriptB);
        work->hasBurst = 1;
    } else {
        if (work->variant != 0) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000C;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0004;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->state = SUCKLERCEPH_STATE_SLUMP_DEATH;
    }
}
