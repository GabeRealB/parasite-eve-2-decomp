/* Part of the Rat library; see rat.h. */

/// Counts the idle-sound timer down; on expiry reloads it with 0x96 plus a
/// random 0-0x7F frames and plays sound 1 at the model's pan and depth.
void ratIdleSound(Task* arg0)
{
    RatWork*  work;
    GfxCoord* coord;
    s32       snd;
    s32       pan;
    u32       random;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->idleSoundTimer--;
    if (work->idleSoundTimer <= 0) {
        random               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->idleSoundTimer = ((random >> 0x10) & 0x7F) + 0x96;
        gRandomLcgState      = random;
        snd                  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070001;
        pan                  = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}
