/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Runs the Grenade Launcher's close strike and its brief forward approach.
///
/// Frame marks are relative to the requested clip's blend length. Attack 4 is live
/// at marks 28..39; marks 28..30 allow 100 parent-coordinate units of forward motion
/// per frame while the player is at least 1000 units away in X/Z. At mark 122,
/// returns to engagement's listening step. Requires live body work and a player
/// matrix in the root's parent frame; borrows one VECTOR block and leaves Y unused.
static void _golemPawnRookLauncherStrikeState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ATTACK            = 4,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_START_FRAME       = 28,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_END_FRAME         = 40,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ADVANCE_END_FRAME = 30,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_STOP_RANGE        = 1000,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_SPEED             = 100,
        GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_FRAMES            = 122,
    };
    GolemPawnRookWork* work;
    GfxCoord*          root;
    VECTOR*            toPlayer;
    s16                blendFrames;
    s32                playerOffsetX;
    s32                playerOffsetZ;
    s32                playerDistance;

    toPlayer    = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work        = actor->work;
    blendFrames = gGolemPawnRookAnimBlendFrames[work->anim];
    root        = actor->extra.tmd->coords;
    if (work->animFrame == blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_START_FRAME) {
        work->strikeBody.key    = damagePackAttackKey(gGolemPawnRookAttacks, GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ATTACK);
        work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else if (work->animFrame == blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_END_FRAME) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    blendFrames = gGolemPawnRookAnimBlendFrames[work->anim];
    if ((work->animFrame >= blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_START_FRAME) && (blendFrames + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_ADVANCE_END_FRAME >= work->animFrame)) {
        playerOffsetX  = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
        toPlayer->vx   = playerOffsetX;
        playerOffsetZ  = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
        toPlayer->vz   = playerOffsetZ;
        playerDistance = SquareRoot0((toPlayer->vx * toPlayer->vx) + (toPlayer->vz * toPlayer->vz));
        if (playerDistance < GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_STOP_RANGE) {
            work->forwardSpeed = 0;
        } else {
            work->forwardSpeed = GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_SPEED;
        }
    } else {
        work->forwardSpeed = 0;
    }
    if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim] + GOLEM_PAWN_ROOK_LAUNCHER_STRIKE_FRAMES) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP;
        work->anim     = GOLEM_PAWN_ROOK_ANIM_LISTEN;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
