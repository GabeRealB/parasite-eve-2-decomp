/* Part of the Diver library; see diver.h. Inline helpers the fragments and
 * the packages' own states use. */

/// Pre-multiplies by a parent basis and normalizes into caller-owned storage.
///
/// Parent and destination are side-effect-free MATRIX pointers; `normalized`
/// is a separate writable MATRIX lvalue. Arguments occur repeatedly. Expands
/// to statements for a braced block, preserving each caller's matrix lifetimes.
#define DIVER_PREMULTIPLY_NORMALIZED_ROTATION(parentBasis, destination, normalized) \
    gte_SetRotMatrix(parentBasis);                                                  \
    MulRotMatrix(destination);                                                      \
    MatrixNormal((destination), &(normalized));                                     \
    *(destination) = (normalized);

/// Composes a joint's rotation into an ancestor's frame, excluding that ancestor.
///
/// Starts with the joint's stored basis, normalizes each intervening parent's
/// basis, then normalizes every product. Coefficients have 12 fractional bits.
/// Returns 1 at a non-NULL `excludedAncestor`, or 0 at the end of the chain;
/// either exit leaves the accumulated rotation. Excluding `gGfxViewCoord`
/// produces a world-space rotation. The joint's own basis is used as stored
/// when no parent product is needed.
///
/// The borrowed chain must be live and acyclic; `rotation` must be a separate,
/// word-aligned writable MATRIX. Only the 3x3 result is valid: normalization
/// copies unspecified translation and alignment bytes too. Changes GTE state.
static __inline__ s32 _diverAccumulateRotation(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor)
{
    MATRIX          normalizedRotation;
    MATRIX          parentRotation;
    const GfxCoord* ancestor;

    ancestor  = joint->parent;
    *rotation = joint->coord;
    while (1) {
        if (ancestor == NULL) {
            return 0;
        }
        if (ancestor == excludedAncestor) {
            return 1;
        }
        // Remove ancestor scale before composing, then normalize the product.
        parentRotation = ancestor->coord;
        MatrixNormal(&parentRotation, &parentRotation);
        DIVER_PREMULTIPLY_NORMALIZED_ROTATION(&parentRotation, rotation, normalizedRotation);
        ancestor = ancestor->parent;
    }
}

/// Converts a world-space rotation into the frame of a joint's parent.
///
/// The parent-to-world basis includes the parent and excludes `gGfxViewCoord`.
/// Its transpose pre-multiplies `worldRotation` without final normalization.
/// The first parent's basis is used as stored; each additional ancestor and
/// product is normalized. Coefficients have 12 fractional bits. Returns `joint`
/// unchanged so the caller can install the rotation through that pointer.
/// The transpose is an inverse only when the accumulated basis is orthonormal.
///
/// Requires a non-NULL parent, a live acyclic chain and a separate writable
/// MATRIX with an initialized 3x3. Leaves the rotation unchanged when the parent
/// is the view or the chain never reaches it. Translation stays intact; the
/// SDK multiply may write alignment bytes. Borrows all inputs and changes GTE state.
static __inline__ GfxCoord* _diverLocalizeRotation(GfxCoord* joint, MATRIX* worldRotation)
{
    MATRIX          parentWorldRotation;
    MATRIX          ancestorRotation;
    MATRIX          normalizedParentRotation;
    MATRIX          transposedParentRotation;
    MATRIX*         parentRotationPtr;
    MATRIX*         ancestorRotationPtr;
    const GfxCoord* ancestor;
    const GfxCoord* viewCoord;

    ancestor = joint->parent;
    if (ancestor != &gGfxViewCoord) {
        parentRotationPtr   = &parentWorldRotation;
        viewCoord           = &gGfxViewCoord;
        ancestorRotationPtr = &ancestorRotation;
        parentWorldRotation = ancestor->coord;
        while (1) {
            ancestor = ancestor->parent;
            if (ancestor == NULL) {
                return joint;
            }
            if (ancestor == viewCoord) {
                // Exclude the view transform when undoing the world-space turn.
                gte_TransposeMatrix(parentRotationPtr, &transposedParentRotation);
                gte_SetRotMatrix(&transposedParentRotation);
                MulRotMatrix(worldRotation);
                break;
            }
            ancestorRotation = ancestor->coord;
            MatrixNormal(&ancestorRotation, &ancestorRotation);
            DIVER_PREMULTIPLY_NORMALIZED_ROTATION(ancestorRotationPtr, &parentWorldRotation, normalizedParentRotation);
        }
    }
    return joint;
}

#undef DIVER_PREMULTIPLY_NORMALIZED_ROTATION

/// Tests whether slot 1 reached or holds a boundary, or took a control jump.
///
/// Reads the last tick's status copied into the live `DiverWork::animStatus`.
/// Returns 0 or 1 without ticking playback or consuming flags. A control jump
/// can continue playback, so a true result need not mean that the clip stopped.
static __inline__ s32 _diverClipHasBoundaryOrJump(Task* task)
{
    DiverWork* work = task->work;

    if ((work->animStatus & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->animStatus & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->animStatus & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

/// Queues a body-clip blend in the carrier's live animation request block.
///
/// `clipIndex` must select a loaded clip supporting body slots 1..14. `rate`
/// is in sixteenths of a frame and narrows to a signed byte when applied;
/// `blendFrames` is in normal-rate frames (0..2047 keeps blend time nonnegative).
/// The carrier's animation driver applies the request on a later update.
static __inline__ void _diverRequestClipBlend(DiverWork* work, s16 clipIndex, s16 rate, s16 blendFrames)
{
    work->animBlend   = blendFrames;
    work->animStep    = rate;
    work->animClip    = clipIndex;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
}
