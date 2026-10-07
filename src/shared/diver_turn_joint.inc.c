/* Part of the Diver library; see diver.h. */

/// Installs only a joint's rotation and refreshes its composed transform.
///
/// Borrows a writable joint and separate readable MATRIX. Copies the nine
/// coefficients, preserving local translation and the matrix's alignment bytes.
/// Marks the cache dirty before composition; the parent chain must be live.
static __inline__ void _diverInstallJointRotation(GfxCoord* joint, const MATRIX* rotation)
{
    memcpy(joint->coord.m, rotation->m, sizeof(joint->coord.m));
    joint->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(joint);
}

/// Turns a joint about world Y and installs the result in its parent's frame.
///
/// `yaw` uses 4096 units per turn. Requires a writable joint beneath the view,
/// a live acyclic chain and an initialized scratch stack with room for a MATRIX.
/// Normalizes each intervening ancestor and accumulated product, turns in world
/// space, then undoes the parent basis without normalizing the final product.
/// Only the local 3x3 is replaced; translation stays intact. Refreshes the cache,
/// changes GTE state and releases its scratch storage before returning.
static void _diverTurnJoint(GfxCoord* coord, s16 yaw)
{
    MATRIX*   rotation;
    GfxCoord* jointToUpdate;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    _diverAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    jointToUpdate = _diverLocalizeRotation(coord, rotation);
    _diverInstallJointRotation(jointToUpdate, rotation);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
