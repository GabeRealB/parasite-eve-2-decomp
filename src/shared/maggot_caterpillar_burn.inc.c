/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Advances the burning damage cycle, periodic blast effects and positional sound.
///
/// Requires a live enemy model with coordinates 0..5 and initialized work.
/// The 80-tick cycle permits flame-contact damage only at frame zero; effects
/// repeat every 12 ticks and sound every 36. Part arguments alternate between
/// 3 and 5, but the blast uses the effect record's retained root coordinate.
/// Audio uses the owning enemy's placement index and signed-byte pan/depth
/// from the composed root.
static void _maggotCaterpillarBurnStep(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_BURN_CYCLE_TICKS  = 80,
        MAGGOT_CATERPILLAR_BURN_EFFECT_TICKS = 12,
        MAGGOT_CATERPILLAR_BURN_SOUND_TICKS  = 36,
        MAGGOT_CATERPILLAR_BURN_SOUND        = 0x401A0005
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    soundKey;
    s32                    audioPan;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    if (++work->burnFrame >= MAGGOT_CATERPILLAR_BURN_CYCLE_TICKS) {
        work->burnFrame = 0;
    }
    if (++work->burnEffectTimer == MAGGOT_CATERPILLAR_BURN_EFFECT_TICKS) {
        work->burnEffectTimer = 0;
        if (work->burnEffectSide == 0) {
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, actor->extra.tmd->coords + 3, NULL, &work->effectArg);
            work->burnEffectSide = 1;
        } else {
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, actor->extra.tmd->coords + 5, NULL, &work->effectArg);
            work->burnEffectSide = 0;
        }
    }
    if (--work->burnSoundTimer <= 0) {
        work->burnSoundTimer = MAGGOT_CATERPILLAR_BURN_SOUND_TICKS;
        soundKey             = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_BURN_SOUND;
        audioPan             = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}
