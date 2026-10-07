/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Plays the fatal fall from standing and hands the body task to its dead state.
///
/// `actor` is a live GOLEM body task whose HP has reached zero. The last hit's
/// side chooses the fall and saved downed pose. Grid collision moves from the
/// standing ground body to the enlarged hurt body until the fall timer expires.
static void _golemPawnRookCollapseState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_FALL_FRONT                  = 0x1A,
        GOLEM_PAWN_ROOK_ANIM_FALL_BEHIND                 = 0x16,
        GOLEM_PAWN_ROOK_COLLAPSE_START                   = 0,
        GOLEM_PAWN_ROOK_COLLAPSE_FALL                    = 1,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FRAMES           = 66,
        GOLEM_PAWN_ROOK_COLLAPSE_FRONT_FRAMES            = 49,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_OFFSET           = 167,
        GOLEM_PAWN_ROOK_COLLAPSE_FRONT_OFFSET            = 265,
        GOLEM_PAWN_ROOK_COLLAPSE_BODY_RADIUS             = 350,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FIRST_CUE_FRAME  = 20,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_SECOND_CUE_FRAME = 44,
        GOLEM_PAWN_ROOK_COLLAPSE_FRONT_CUE_FRAME         = 25,
        GOLEM_PAWN_ROOK_COLLAPSE_FIRST_CUE_BASE          = 12,
        GOLEM_PAWN_ROOK_COLLAPSE_IMPACT_CUE_BASE         = 8,
        GOLEM_PAWN_ROOK_KNOCKDOWN_START                  = 1,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN                 = 2,
    };

    GolemPawnRookWork* work;
    GfxCoord*          root;
    s32                soundId;
    s16                step;

    work = actor->work;
    root = actor->extra.tmd->coords;
    step = work->step;

    switch (step) {
        case GOLEM_PAWN_ROOK_COLLAPSE_START:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_PAWN_ROOK_ANIM_FALL_BEHIND;
                work->step            = GOLEM_PAWN_ROOK_COLLAPSE_FALL;
                work->downedPose      = GOLEM_PAWN_ROOK_DOWNED_BEHIND;
                work->timer           = GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FRAMES;
                work->hurtBody.pos.vz = -GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_OFFSET;
            } else {
                work->anim            = GOLEM_PAWN_ROOK_ANIM_FALL_FRONT;
                work->step            = GOLEM_PAWN_ROOK_COLLAPSE_FALL;
                work->downedPose      = GOLEM_PAWN_ROOK_DOWNED_FRONT;
                work->timer           = GOLEM_PAWN_ROOK_COLLAPSE_FRONT_FRAMES;
                work->hurtBody.pos.vz = GOLEM_PAWN_ROOK_COLLAPSE_FRONT_OFFSET;
            }
            // The fallen hurt body now resolves the room grid in place of the foot sphere.
            work->hurtBody.radius                             = GOLEM_PAWN_ROOK_COLLAPSE_BODY_RADIUS;
            work->forwardSpeed                                = 0;
            work->turnRate                                    = 0;
            work->knockdownStage                              = GOLEM_PAWN_ROOK_KNOCKDOWN_START;
            work->hurtBody.flags                             |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags                           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            ((Enemy*)actor->spawnArg2.pointer)->reactionFlags = 0;
            work->fallingDown                                 = 1;
            break;
        case GOLEM_PAWN_ROOK_COLLAPSE_FALL:
            if (work->knockdownStage == GOLEM_PAWN_ROOK_KNOCKDOWN_START) {
                work->knockdownStage = GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN;
            }
            if (work->downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                if (work->animFrame == GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FIRST_CUE_FRAME) {
                    s32 audioPan;

                    soundId = gGolemPawnRookVoiceCues[work->soundSet + GOLEM_PAWN_ROOK_COLLAPSE_FIRST_CUE_BASE] |
                              ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                    audioPan = (s8)worldCoordGetOriginAudioPan(root);

                    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                }
                if (work->animFrame == GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_SECOND_CUE_FRAME) {
                    s32 audioPan;

                    soundId = gGolemPawnRookVoiceCues[work->soundSet + GOLEM_PAWN_ROOK_COLLAPSE_IMPACT_CUE_BASE] |
                              ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                    audioPan = (s8)worldCoordGetOriginAudioPan(root);

                    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                }
            } else if (work->animFrame == GOLEM_PAWN_ROOK_COLLAPSE_FRONT_CUE_FRAME) {
                s32 audioPan;

                soundId = gGolemPawnRookVoiceCues[work->soundSet + GOLEM_PAWN_ROOK_COLLAPSE_IMPACT_CUE_BASE] |
                          ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                audioPan = (s8)worldCoordGetOriginAudioPan(root);

                sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            work->timer--;
            if (work->timer <= 0) {
                actor->state      = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
                work->step        = GOLEM_PAWN_ROOK_COLLAPSE_START;
                work->fallingDown = 0;
            }
            break;
    }
}
