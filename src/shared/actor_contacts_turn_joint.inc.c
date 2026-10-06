/* Actor-render joint rotation carried by actor_contacts.h. */

/// Installs a parent-space joint rotation and refreshes the composed transform.
static inline void _actorRenderInstallJointRotation(GfxCoord* joint, const MATRIX* localRotation)
{
    // Only the rotation is valid after normalization; keep the joint's translation.
    memcpy(joint->coord.m, localRotation->m, sizeof(joint->coord.m));
    joint->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(joint);
}

/// Adds a world-space yaw to a model joint's current rotation.
///
/// `yawDelta` is a signed angle in 4096 units per turn, applied about world Y;
/// it is neither an absolute heading nor a turn about the joint's local Y.
/// Rotation coefficients have 12 fractional bits. Ancestor products are
/// normalized; the result replaces only the joint's 3x3 rotation, preserving
/// its local translation and optional Euler state, and its composed cache is
/// refreshed before returning.
///
/// `joint` must be writable, with a non-NULL parent and a live, acyclic chain
/// reaching `gGfxViewCoord` as a strict ancestor. The nodes remain owned by
/// the caller. The initialized scratch stack must have word-aligned room for
/// one `MATRIX`; that temporary is released before returning. GTE working
/// registers change.
static void _actorRenderYawJointInWorld(GfxCoord* joint, s16 yawDelta)
{
    MATRIX* worldRotation;

    worldRotation = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    _actorRenderAccumulateRotation(joint, worldRotation, &gGfxViewCoord);
    RotMatrixY(yawDelta, worldRotation);
    _actorRenderLocalizeRotation(joint, worldRotation);
    _actorRenderInstallJointRotation(joint, worldRotation);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
