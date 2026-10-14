/* Shared No. 9 GOLEM implementation, carried by actor_510900 and actor_521100. */

/// Scratch-stack block of the No. 9 GOLEM's head turned to aim at the player.
///
/// The aim reserves one block a tick, works out where the player stands from
/// the head in the frame of the model's root, limits that offset and builds
/// the head's rotation along it; the block is released before the aim
/// returns and nothing reads it afterwards.
typedef struct {
    MATRIX headWorld; // The head part's transform taken out of the view: its rotation and position along the world's axes. Only the translation is read
    VECTOR toTarget;  // Offset from the head to the point 0x600 above the player's root (toward negative Y), along the world's axes; `pad` is never written
    VECTOR aim;       // `toTarget` turned into the frame of the model's root, then limited to [-0x400, 0x400] on X, [-0x300, 0x300] on Y and at least 0x200 on Z: the direction the head is turned along
} _No9GolemHeadAimScratch;
STATIC_ASSERT_SIZEOF(_No9GolemHeadAimScratch, 0x40);

/// Turns the GOLEM's head toward a bounded player direction in root-local space.
///
/// Requires a live model with coordinate 4, a cached head transform in the view
/// frame, and the player's world transform. Removes the view from that cache,
/// aims 1536 world units above the player, and rotates the difference by the
/// root's local rotation transpose. Clamps X/Y to +/-1024/768 and forward Z
/// to at least 512 before replacing the head rotation. Does not compose or
/// invalidate either coordinate; the caller handles composition afterwards.
static void _no9GolemAimHead(const Task* actor)
{
    enum {
        NO9_GOLEM_HEAD_COORD_INDEX = 4,
        NO9_GOLEM_HEAD_TARGET_RISE = 1536,
        NO9_GOLEM_HEAD_AIM_X_LIMIT = 1024,
        NO9_GOLEM_HEAD_AIM_Y_LIMIT = 768,
        NO9_GOLEM_HEAD_AIM_Z_MIN   = 512
    };
    _No9GolemHeadAimScratch* scratch;
    GfxCoord*                rootCoord;
    GfxCoord*                headCoord;
    s32                      headTargetBaseY;

    rootCoord = actor->extra.tmd->coords;
    headCoord = &rootCoord[NO9_GOLEM_HEAD_COORD_INDEX];
    SCRATCH_STACK_RESERVE_BYTES(sizeof(_No9GolemHeadAimScratch));
    scratch = SCRATCH_STACK_CURSOR(_No9GolemHeadAimScratch);

    // Remove the view before measuring the world-space target offset.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &headCoord->workm, &scratch->headWorld);
    scratch->toTarget.vx = gPlayerStatus.coordMtx->t[0] - scratch->headWorld.t[0];
    headTargetBaseY      = scratch->headWorld.t[1] + NO9_GOLEM_HEAD_TARGET_RISE;
    scratch->toTarget.vy = gPlayerStatus.coordMtx->t[1] - headTargetBaseY;
    scratch->toTarget.vz = gPlayerStatus.coordMtx->t[2] - scratch->headWorld.t[2];
    // Clamp the direction in the root frame before rebuilding the head rotation.
    ApplyTransposeMatrixLV(&rootCoord->coord, &scratch->toTarget, &scratch->aim);

    if (scratch->aim.vx < -NO9_GOLEM_HEAD_AIM_X_LIMIT) {
        scratch->aim.vx = -NO9_GOLEM_HEAD_AIM_X_LIMIT;
    } else if (scratch->aim.vx > NO9_GOLEM_HEAD_AIM_X_LIMIT) {
        scratch->aim.vx = NO9_GOLEM_HEAD_AIM_X_LIMIT;
    }
    if (scratch->aim.vy < -NO9_GOLEM_HEAD_AIM_Y_LIMIT) {
        scratch->aim.vy = -NO9_GOLEM_HEAD_AIM_Y_LIMIT;
    } else if (scratch->aim.vy > NO9_GOLEM_HEAD_AIM_Y_LIMIT) {
        scratch->aim.vy = NO9_GOLEM_HEAD_AIM_Y_LIMIT;
    }
    if (scratch->aim.vz < NO9_GOLEM_HEAD_AIM_Z_MIN) {
        scratch->aim.vz = NO9_GOLEM_HEAD_AIM_Z_MIN;
    }
    gfxBuildDirectionRotation(&scratch->aim, &headCoord->coord, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_No9GolemHeadAimScratch));
}
