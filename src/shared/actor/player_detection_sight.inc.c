/* Part of the player detection library; see player_detection.h. */

/// Returns 1 when an enabled room sight occluder separates the actor and player.
///
/// Both tasks must have live model roots with world-space local translations.
/// Each root is raised 1000 game units along negative Y, narrowed to signed
/// halfwords, then transformed by the refreshed view matrix. The resulting
/// view-space points are queried by `worldCollisionSegmentOccluded`.
/// Its nonzero segment and normalization bounds apply after transformation.
///
/// Uses 172 bytes at peak on the initialized scratch stack, clear of the roots.
/// Releases all reservations before return and retains no pointers. Refreshes
/// the view coordinate's composition cache and clobbers GTE state.
static s32 _playerDetectionSightBlocked(const Task* actor)
{
    enum { PLAYER_DETECTION_EYE_HEIGHT = 1000 };
    Task*                        player;
    PlayerDetectionSightScratch* scratchTop;
    PlayerDetectionSightScratch* scratch;
    SVECTOR*                     localEye;
    SVECTOR*                     eyeInput;
    SVECTOR*                     actorEye;

    // Transform the player's raised root into the occluders' view frame.
    player                                            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratchTop                                        = SCRATCH_STACK_CURSOR(PlayerDetectionSightScratch);
    localEye                                          = &scratchTop[-1].localEye;
    scratch                                           = scratchTop - 1;
    scratch->localEye.vx                              = player->extra.tmd->coords->coord.t[0];
    scratch->localEye.vy                              = player->extra.tmd->coords->coord.t[1] - PLAYER_DETECTION_EYE_HEIGHT;
    SCRATCH_STACK_CURSOR(PlayerDetectionSightScratch) = scratch;
    scratch->localEye.vz                              = player->extra.tmd->coords->coord.t[2];
    actorRenderComposeCoord(&gGfxViewCoord);
    // Keep the GTE input alias separate across the two view compositions.
    eyeInput = localEye;
/// Applies the composed view matrix to a raised root point.
///
/// Captures `gGfxViewCoord`; compose it before each use. `input` and `output`
/// are SVECTOR pointers, and `point` is the SVECTOR lvalue at `output`.
/// Arguments must be side-effect-free; `point` is evaluated repeatedly.
/// Use as a standalone sequence here. Input may alias output; pad is unchanged.
/// Clobbers GTE state.
#define PLAYER_DETECTION_TRANSFORM_EYE(input, output, point) \
    gte_SetRotMatrix(&gGfxViewCoord.workm);                  \
    gte_ldv0(input);                                         \
    gte_rtv0();                                              \
    gte_stsv(output);                                        \
    (point).vx += gGfxViewCoord.workm.t[0];                  \
    (point).vy += gGfxViewCoord.workm.t[1];                  \
    (point).vz += gGfxViewCoord.workm.t[2]
    PLAYER_DETECTION_TRANSFORM_EYE(eyeInput, &scratch->playerEye, scratch->playerEye);

    // Reuse the input vector for the actor, then test between the raised points.
    scratch->localEye.vx = actor->extra.tmd->coords->coord.t[0];
    scratch->localEye.vy = actor->extra.tmd->coords->coord.t[1] - PLAYER_DETECTION_EYE_HEIGHT;
    scratch->localEye.vz = actor->extra.tmd->coords->coord.t[2];
    actorRenderComposeCoord(&gGfxViewCoord);
    actorEye = &scratchTop[-1].actorEye;
    PLAYER_DETECTION_TRANSFORM_EYE(eyeInput, actorEye, scratch->actorEye);
#undef PLAYER_DETECTION_TRANSFORM_EYE
    scratch->blocked = worldCollisionSegmentOccluded(&scratch->playerEye, actorEye);
    // The return reads released bytes immediately, before any call can reuse them.
    SCRATCH_STACK_RELEASE_BLOCK(PlayerDetectionSightScratch);
    return scratch->blocked;
}
