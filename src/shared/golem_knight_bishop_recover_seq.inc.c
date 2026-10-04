/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Leaves the golem standing exposed for a moment, then vanishes it. Step 0
/// takes the standing animation 0xB and rolls `timer` from the
/// `gRandomLcgState` LCG (`GOLEM_KNIGHT_BISHOP_RECOVER_DELAY` plus 0..0x1F);
/// unless `hitCooldown` is running it also arms `hurtBody`. Step 1 counts
/// `timer` down and, on the frame it runs out, starts the vanish with its
/// sound, panned and depth-attenuated from the display object, and hands over
/// to the idle sequence.
void golemKnightBishopRecoverSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    pan;
    u32                    random;
    s16                    timer;

    SCRATCH_STACK_RESERVE_BYTES(8);
    work  = arg0->work;
    state = work->step;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->anim      = 0xB;
            work->step      = 1;
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            work->timer     = ((random >> 16) & 0x1F) + GOLEM_KNIGHT_BISHOP_RECOVER_DELAY;
            if (work->hitCooldown == 0) {
                work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->hurtBody.key    = work->actorId | 0x30000;
            }
            break;
        case 1:
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0xA;
                work->sequence               = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step                   = 0;
                work->colorBlendFadeFrames   = 5;
                work->flickerTimer           = 0;
                work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan                          = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->vanishSound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
