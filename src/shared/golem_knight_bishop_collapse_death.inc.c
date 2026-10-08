/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Falls dead from standing and hands the task to its corpse handler.
///
/// The last hit's side selects the fall clip, saved downed pose and impact frame.
/// Grid participation moves from the root's ground sphere to the shifted hurt
/// sphere. The signed frame countdown includes the first update after setup.
static void _golemKnightBishopCollapseDeathSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_COLLAPSE_START = 0,
        GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL  = 1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    sound;
    s32                    audioPan;
    s32                    impactFrame;
    s16                    framesLeft;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_COLLAPSE_START:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND;
                work->step            = GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND;
                work->timer           = GOLEM_KNIGHT_BISHOP_FALL_BEHIND_FRAMES;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND_OFFSET_Z;
            } else {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT;
                work->step            = GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT;
                work->timer           = GOLEM_KNIGHT_BISHOP_FALL_FRONT_FRAMES;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT_OFFSET_Z;
            }
            work->hurtBody.radius        = GOLEM_KNIGHT_BISHOP_HURT_RADIUS;
            work->knockdownStage         = GOLEM_KNIGHT_BISHOP_FALL_STARTED;
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
            work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
            work->reactionLock           = GOLEM_KNIGHT_BISHOP_REACTION_FALLING;
            work->forwardSpeed           = 0;
            work->hurtBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL:
            if (work->knockdownStage == step) {
                work->knockdownStage = GOLEM_KNIGHT_BISHOP_FALL_SETTLED;
            }
            impactFrame = GOLEM_KNIGHT_BISHOP_FALL_FRONT_IMPACT_FRAME;
            if (work->downedPose == step) {
                impactFrame = GOLEM_KNIGHT_BISHOP_FALL_BEHIND_IMPACT_FRAME;
            }
            if (work->animFrame == impactFrame) {
                sound    = gGolemKnightBishopAnimCues[work->soundSet + 8] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(sound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                task->state        = GOLEM_KNIGHT_BISHOP_TASK_DEAD;
                work->step         = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->reactionLock = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
            }
            break;
    }
}
