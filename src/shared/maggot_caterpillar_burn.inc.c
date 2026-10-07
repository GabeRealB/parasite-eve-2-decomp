#include "gameplay/room_effects.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Status-effect step, run every frame while `burning` is set. `burnFrame`
/// cycles through 0x50 frames; every 12 frames effect 3 is spawned, alternating
/// between model nodes 3 and 5; every 0x24 frames sound 0x401A0005 is played
/// with the top nibble of the context's `field_8` in bits 8-11, panned to the
/// actor.
void maggotCaterpillarBurnStep(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    sound;
    s32                    pan;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (++work->burnFrame >= 0x50) {
        work->burnFrame = 0;
    }
    if (++work->burnEffectTimer == 0xC) {
        work->burnEffectTimer = 0;
        if (work->burnEffectSide == 0) {
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, arg0->extra.tmd->coords + 3, NULL, &work->effectArg);
            work->burnEffectSide = 1;
        } else {
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, arg0->extra.tmd->coords + 5, NULL, &work->effectArg);
            work->burnEffectSide = 0;
        }
    }
    if (--work->burnSoundTimer <= 0) {
        work->burnSoundTimer = 0x24;
        sound                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0005;
        pan                  = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(coord));
    }
}
