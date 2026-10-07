/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Reacts to the player's action noise while the GOLEM is idle or patrolling.
///
/// `actor` is a live GOLEM body task. Within 1500 horizontal game units any
/// noisy action spots the player. Farther away, loud actions always start a
/// listening animation; footsteps and active PE do so only within 3000 units.
/// This check does not test walls or change `playerSpotted` back to zero.
static void _golemPawnRookCheckPlayerNoise(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_NOISE_SPOT_RANGE         = 1500,
        GOLEM_PAWN_ROOK_NOISE_LISTEN_STEP        = 1,
        GOLEM_PAWN_ROOK_QUIET_NOISE_LISTEN_RANGE = 3000,
    };

    GolemPawnRookWork* work;
    GfxCoord*          root;
    s32                playerDx;
    s32                playerDistance;
    s32                playerDz;
    s32                heardDistantAction;
    VECTOR*            toPlayer;

    root                         = actor->extra.tmd->coords;
    work                         = actor->work;
    toPlayer                     = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    toPlayer->vx                 = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
    toPlayer->vy                 = 0;
    playerDz                     = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
    toPlayer->vz                 = playerDz;
    playerDx                     = toPlayer->vx;
    heardDistantAction           = 0;
    SCRATCH_STACK_CURSOR(VECTOR) = toPlayer;
    playerDistance               = SquareRoot0((playerDx * playerDx) + (playerDz * playerDz));
    if (playerDistance < GOLEM_PAWN_ROOK_NOISE_SPOT_RANGE) {
        if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_PE_CAST_OTHER | SCENE_COMBAT_ACTION_FOOTSTEP)) {
            work->playerSpotted = 1;
        }
    } else {
        if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
            heardDistantAction = 1;
        }
        if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_FOOTSTEP)) && (playerDistance < GOLEM_PAWN_ROOK_QUIET_NOISE_LISTEN_RANGE)) {
            heardDistantAction = 1;
        }
        if (heardDistantAction != 0) {
            work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->timer        = 0;
            work->step         = GOLEM_PAWN_ROOK_NOISE_LISTEN_STEP;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
