/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Stands exposed for a random recovery interval, then vanishes and idles.
///
/// The interval is the kind's recovery base plus 0..31 frame updates. The hurt
/// sphere is rearmed only when its hit cooldown has expired. The retained
/// eight-byte scratch reservation contains no accessed object; its purpose is
/// unproven.
static void _golemKnightBishopRecoverSeq(Task* task)
{
    enum { GOLEM_KNIGHT_BISHOP_RECOVERY_SCRATCH_BYTES = 8 };
    enum {
        GOLEM_KNIGHT_BISHOP_RECOVER_START = 0,
        GOLEM_KNIGHT_BISHOP_RECOVER_WAIT  = 1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    audioPan;
    u32                    randomDraw;
    s16                    framesLeft;

    SCRATCH_STACK_RESERVE_BYTES(GOLEM_KNIGHT_BISHOP_RECOVERY_SCRATCH_BYTES);
    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_RECOVER_START:
            work->anim      = GOLEM_KNIGHT_BISHOP_ANIM_RECOVER;
            work->step      = GOLEM_KNIGHT_BISHOP_RECOVER_WAIT;
            randomDraw      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomDraw;
            work->timer     = ((randomDraw >> 16) & 0x1F) + GOLEM_KNIGHT_BISHOP_RECOVER_DELAY;
            if (work->hitCooldown == 0) {
                work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_RECOVER_WAIT:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0xA;
                work->sequence               = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step                   = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->colorBlendFadeFrames   = 5;
                work->flickerTimer           = 0;
                work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan                     = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(work->vanishSound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(GOLEM_KNIGHT_BISHOP_RECOVERY_SCRATCH_BYTES);
}
