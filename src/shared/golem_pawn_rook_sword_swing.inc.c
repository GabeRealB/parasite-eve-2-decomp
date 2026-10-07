/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Performs the close-range Beam Sword swing and its recovery.
///
/// `actor` is a live Beam Sword GOLEM body task. The swing tracks the player,
/// advances briefly when at least 1000 game units away, and enables the weapon
/// collision only during the strike. Strike timing is relative to the animation
/// blend; recovery returns to the engage behaviour's listening step.
static void _golemPawnRookSwordSwingState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_SWORD_RECOVER        = 9,
        GOLEM_PAWN_ROOK_SWING_STRIKE              = 0,
        GOLEM_PAWN_ROOK_SWING_RECOVER             = 1,
        GOLEM_PAWN_ROOK_SWING_ADVANCE_START_FRAME = 34,
        GOLEM_PAWN_ROOK_SWING_ADVANCE_END_FRAME   = 38,
        GOLEM_PAWN_ROOK_SWING_STOP_RANGE          = 1000,
        GOLEM_PAWN_ROOK_SWING_SPEED               = 132,
        GOLEM_PAWN_ROOK_SWING_TURN_RATE           = 20,
        GOLEM_PAWN_ROOK_SWING_STRIKE_START_FRAME  = 32,
        GOLEM_PAWN_ROOK_SWING_SOUND_FRAME         = 33,
        GOLEM_PAWN_ROOK_SWING_STRIKE_END_FRAME    = 39,
        GOLEM_PAWN_ROOK_SWING_RECOVERY_FRAMES     = 94,
        GOLEM_PAWN_ROOK_SWING_ATTACK              = 0,
    };

    s16                blendFrames;
    s16                step;
    s16                animFrame;
    s32                playerDz;
    s32                soundId;
    s32                playerDx;
    s32                audioPan;
    GolemPawnRookWork* work;
    GfxCoord*          root;
    VECTOR*            toPlayer;

    toPlayer = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work     = actor->work;
    step     = work->step;
    root     = actor->extra.tmd->coords;
    // The strike window begins after the animation blend; recovery uses absolute frames.
    switch (step) {
        case GOLEM_PAWN_ROOK_SWING_STRIKE:
            toPlayer->vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            playerDz     = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            toPlayer->vz = playerDz;
            blendFrames  = gGolemPawnRookAnimBlendFrames[work->anim];
            animFrame    = work->animFrame;
            if ((animFrame >= (blendFrames + GOLEM_PAWN_ROOK_SWING_ADVANCE_START_FRAME)) &&
                ((blendFrames + GOLEM_PAWN_ROOK_SWING_ADVANCE_END_FRAME) >= animFrame)) {
                playerDx = toPlayer->vx;
                if (SquareRoot0((playerDx * playerDx) + (playerDz * playerDz)) >= GOLEM_PAWN_ROOK_SWING_STOP_RANGE) {
                    work->forwardSpeed = GOLEM_PAWN_ROOK_SWING_SPEED;
                } else {
                    work->forwardSpeed = 0;
                }
            } else {
                work->forwardSpeed = 0;
            }
            work->turnRate  = GOLEM_PAWN_ROOK_SWING_TURN_RATE;
            work->targetYaw = ratan2((s16)toPlayer->vx, (s16)toPlayer->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (work->animFrame == (gGolemPawnRookAnimBlendFrames[work->anim] + GOLEM_PAWN_ROOK_SWING_STRIKE_START_FRAME)) {
                work->strikeBody.flags = work->strikeBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->strikeBody.key   = damagePackAttackKey(gGolemPawnRookAttacks, GOLEM_PAWN_ROOK_SWING_ATTACK);
            }
            if (work->animFrame == (gGolemPawnRookAnimBlendFrames[work->anim] + GOLEM_PAWN_ROOK_SWING_SOUND_FRAME)) {
                soundId  = gGolemPawnRookSwingCue | ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                audioPan = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame >= (gGolemPawnRookAnimBlendFrames[work->anim] + GOLEM_PAWN_ROOK_SWING_STRIKE_END_FRAME)) {
                work->step             = GOLEM_PAWN_ROOK_SWING_RECOVER;
                work->anim             = GOLEM_PAWN_ROOK_ANIM_SWORD_RECOVER;
                work->strikeBody.flags = work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case GOLEM_PAWN_ROOK_SWING_RECOVER:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->animFrame >= GOLEM_PAWN_ROOK_SWING_RECOVERY_FRAMES) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP;
                work->anim     = GOLEM_PAWN_ROOK_ANIM_LISTEN;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
