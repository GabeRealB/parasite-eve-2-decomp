/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the light flinch. Step 0 takes animation 8, clears `forwardSpeed` and
/// goes to step 1; unless `flickerStage` is 1 it also starts the vanish with
/// its sound, panned and depth-attenuated from the display object. Step 1
/// waits for `animFrame` to reach 0x37 and then hands over to the idle
/// sequence, or, when `flickerStage` was 1, sets it to 2 and hands over to
/// the recover sequence.
void golemKnightBishopLightFlinchSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    pan;

    work  = arg0->work;
    state = work->step;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->anim         = 8;
            work->step         = 1;
            work->forwardSpeed = 0;
            if (work->flickerStage != 1) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x1E;
                work->colorBlendFadeFrames   = 0xF;
                work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan                          = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->vanishSound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                break;
            }
            break;
        case 1:
            if (work->animFrame >= 0x37) {
                if (work->flickerStage == state) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = 2;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = 0;
            }
            break;
    }
}
