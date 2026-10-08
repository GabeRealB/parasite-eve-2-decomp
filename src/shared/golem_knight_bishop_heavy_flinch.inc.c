/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Plays the side-dependent heavy hit reaction, then idles or recovers.
///
/// A non-flicker hit vanishes during its reaction; a flicker hit remains exposed
/// and advances the flicker stage on completion. Front and rear clips finish
/// at animation frames 80 and 59 respectively.
static void _golemKnightBishopHeavyFlinchSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_START  = 0,
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_FRONT  = 1,
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_BEHIND = 2,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    audioPan;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_START:
            if (work->hitFromFront == 1) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_FRONT;
                work->step = GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_FRONT;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_BEHIND;
                work->step = GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_BEHIND;
            }
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
        case GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_FRONT:
            if (work->animFrame >= 0x50) {
                if (work->flickerStage == step) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = GOLEM_KNIGHT_BISHOP_FLICKER_RECOVERED;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_BEHIND:
            if (work->animFrame >= 0x3B) {
                if (work->flickerStage == GOLEM_KNIGHT_BISHOP_FLICKER_HIT) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = step;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
    }
}
