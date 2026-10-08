/* Part of the Rat library; see rat.h. */

/// Counts down the idle audio delay and queues the next spatial sound.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`. On expiry the delay
/// becomes 150..277 update calls. Placement index selects the script instance;
/// pan and depth retain only their signed low bytes.
static void _ratIdleSound(Task* actor)
{
    enum {
        RAT_IDLE_SOUND_BASE_FRAMES = 150,
    };

    RatWork*  work;
    GfxCoord* rootCoord;
    s32       soundId;
    s32       audioPan;
    u32       delayRandom;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    work->idleSoundTimer--;
    if (work->idleSoundTimer <= 0) {
        delayRandom          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->idleSoundTimer = ((delayRandom >> 0x10) & 0x7F) + RAT_IDLE_SOUND_BASE_FRAMES;
        gRandomLcgState      = delayRandom;
        soundId              = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_IDLE;
        audioPan             = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
}
