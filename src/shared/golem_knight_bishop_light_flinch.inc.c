/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Plays the light hit reaction, then idles or recovers.
///
/// The reaction finishes at animation frame 55. A non-flicker hit vanishes during
/// the clip; a flicker hit advances its flicker stage and enters exposed recovery.
static void _golemKnightBishopLightFlinchSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_START = 0,
        GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_WAIT  = 1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    audioPan;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_START:
            work->anim         = GOLEM_KNIGHT_BISHOP_ANIM_LIGHT_FLINCH;
            work->step         = GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_WAIT;
            work->forwardSpeed = 0;
            if (work->flickerStage != GOLEM_KNIGHT_BISHOP_FLICKER_HIT) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x1E;
                work->colorBlendFadeFrames   = 0xF;
                work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan                     = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(work->vanishSound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                break;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_WAIT:
            if (work->animFrame >= 0x37) {
                if (work->flickerStage == step) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = GOLEM_KNIGHT_BISHOP_FLICKER_RECOVERED;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
    }
}
