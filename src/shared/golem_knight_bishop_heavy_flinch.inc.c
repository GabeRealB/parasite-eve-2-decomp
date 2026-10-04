/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the heavy flinch. Step 0 takes animation 9 or 0xA, whichever
/// `hitFromFront` selects, and goes to step 1 or 2 to match; unless
/// `flickerStage` is 1 it also starts the vanish with its sound, panned and
/// depth-attenuated from the display object. Steps 1 and 2 wait out their own
/// animation - `animFrame` at 0x50 and 0x3B - and then hand over to the idle
/// sequence, or, when `flickerStage` was 1, set it to 2 and hand over to the
/// recover sequence.
void golemKnightBishopHeavyFlinchSeq(Task* arg0)
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
            if (work->hitFromFront == 1) {
                work->anim = 9;
                work->step = 1;
            } else {
                work->anim = 0xA;
                work->step = 2;
            }
            work->forwardSpeed = 0;
            if (work->flickerStage != 1) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x1E;
                work->colorBlendFadeFrames   = 0xF;
                work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan                          = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->vanishSound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                break;
            }
            break;
        case 1:
            if (work->animFrame >= 0x50) {
                if (work->flickerStage == state) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = 2;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = 0;
            }
            break;
        case 2:
            if (work->animFrame >= 0x3B) {
                if (work->flickerStage == 1) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = state;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = 0;
            }
            break;
    }
}
