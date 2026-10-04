/* Part of the Diver library; see diver.h. Inline helpers the fragments and
 * the packages' own states use. */

/// Accumulate `arg0`'s parent chain into `arg1`: seed it with the node's own
/// rotation, then pre-multiply by each (renormalised) ancestor up to but not
/// including `arg2`, renormalising after every step. Returns whether the walk
/// stopped on `arg2` rather than running off the end of the chain.
static __inline__ s32 diverAccumulateRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2)
{
    MATRIX    normal;
    MATRIX    matrix;
    GfxCoord* coord;

    coord = arg0->parent;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        matrix = coord->coord;
        MatrixNormal(&matrix, &matrix);
        gte_SetRotMatrix(&matrix);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &normal);
        *arg1 = normal;
        coord = coord->parent;
    }
}

/// Undo the parent chain again, turning the world-space rotation in `arg1`
/// back into one relative to `arg0`'s parent: accumulate the chain *above* the
/// parent, transpose it (the 3x3 inverse of a rotation) and pre-multiply.
/// Nothing to do when the parent is already the view coordinate.
///
/// Returns `arg0` so the caller stores through the returned pointer; the copy
/// GCC emits where the exits merge is what gives the store base its own
/// pseudo. Three details here are matching requirements rather than style:
/// the early `return arg0;` on the end-of-chain exit (it is what lifts `arg0`
/// past the scratch pointers in global-alloc's priority order, so it keeps
/// `$s3`), and the `mp` / `lp` pointer variables, whose declarations must
/// precede `view` so their pseudos out-rank it when the two tie.
static __inline__ GfxCoord* diverLocalizeRotation(GfxCoord* arg0, MATRIX* arg1)
{
    MATRIX    matrix;
    MATRIX    local;
    MATRIX    normal;
    MATRIX    transposed;
    MATRIX*   mp;
    MATRIX*   lp;
    GfxCoord* coord;
    GfxCoord* view;

    coord = arg0->parent;
    if (coord != &gGfxViewCoord) {
        mp     = &matrix;
        view   = &gGfxViewCoord;
        lp     = &local;
        matrix = coord->coord;
        while (1) {
            coord = coord->parent;
            if (coord == NULL) {
                return arg0;
            }
            if (coord == view) {
                gte_TransposeMatrix(mp, &transposed);
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(arg1);
                break;
            }
            local = coord->coord;
            MatrixNormal(&local, &local);
            gte_SetRotMatrix(lp);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return arg0;
}

/// Whether the clip the body slots play has ended: slot 1 reached its
/// boundary, followed a jump or settled during the last frame's ticks, as the
/// package's animation step copied into `animStatus`.
///
/// The three bits are tested singly and the work block is loaded afresh from
/// the task; both are matching requirements.
static __inline__ s32 diverClipEnded(Task* task)
{
    DiverWork* work = task->work;

    if ((work->animStatus & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->animStatus & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->animStatus & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}
