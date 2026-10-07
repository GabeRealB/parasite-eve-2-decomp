/* Part of the Diver library; see diver.h. */

/// Replaces a joint's local rotation and refreshes its full-chain composed transform.
///
/// Borrows a writable joint and a separate, word-aligned `localRotation` whose
/// nine signed Q12 coefficients already use the joint's parent frame. Copies
/// only the 3x3 basis, preserving local translation, alignment bytes and Euler
/// storage. The source may leave its other MATRIX fields unspecified. Marks
/// the cache dirty and composes using the current pass without advancing it.
/// Requires a live acyclic parent chain and `actorRenderComposeCoord`'s cache
/// contract; composition may update ancestor caches and changes GTE registers.
static __inline__ void _actorRenderInstallJointRotation(GfxCoord* joint, const MATRIX* localRotation)
{
    memcpy(joint->coord.m, localRotation->m, sizeof(joint->coord.m));
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
    _actorRenderInstallJointRotation(jointToUpdate, rotation);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
