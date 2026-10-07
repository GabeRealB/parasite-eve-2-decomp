/* Part of the coord math library; see coord_math.h. */

/// Transforms a staged point into its parent's space, narrowing XYZ to halfwords.
///
/// `localToParent` is a stable, side-effect-free pointer to a live, word-aligned
/// matrix with 12-fractional-bit coefficients and game-unit translation; it is
/// evaluated twice. `point` is a word-aligned SVECTOR local, separate from the
/// matrix, named once for the GTE load and once per XYZ assignment. It must not
/// be named `transformedPoint` or `gteFlags`, the macro's block-local temporaries.
/// Captures no caller identifiers. GTE flags are captured and discarded; the
/// point's fourth halfword is preserved. The expansion is one statement.
#define ACTOR_RENDER_TRANSFORM_POINT_TO_PARENT(localToParent, point) \
    do {                                                             \
        VECTOR transformedPoint;                                     \
        s32    gteFlags;                                             \
        gte_SetTransMatrix((localToParent));                         \
        gte_SetRotMatrix((localToParent));                           \
        gte_ldv0(&(point));                                          \
        gte_rtv0tr();                                                \
        gte_stlvnl(&transformedPoint);                               \
        gte_stflg(&gteFlags);                                        \
        (point).vx = transformedPoint.vx;                            \
        (point).vy = transformedPoint.vy;                            \
        (point).vz = transformedPoint.vz;                            \
    } while (0)

/// Transforms a local point into world space through an actor's parent chain.
///
/// `startCoord` and its borrowed ancestors must stay live and form an acyclic
/// chain. `point` supplies a writable, halfword-aligned `SVECTOR` in signed game
/// coordinate units, separate from the hierarchy. Each local matrix maps the
/// point into its parent's space; the GTE's long result narrows to signed
/// halfwords after every step. Cached matrices and composition stamps are unused.
///
/// Returns 1 and replaces only XYZ when the walk reaches `gGfxViewCoord` with
/// a non-NULL parent, excluding the view's matrix. Returns 0 without changing
/// `point` if any visited node has no parent, including the view itself.
/// Overwrites the GTE rotation, translation, vector, result and flag registers;
/// its overflow flags are captured but do not affect the return value.
static s32 _actorRenderTransformPointToWorld(const GfxCoord* startCoord, SVECTOR* point)
{
    SVECTOR         parentPoint;
    const GfxCoord* currentCoord;

    // Stage the walk so an incomplete chain leaves the caller's point intact.
    currentCoord   = startCoord;
    parentPoint.vx = point->vx;
    parentPoint.vy = point->vy;
    parentPoint.vz = point->vz;
    while (1) {
        if (currentCoord->parent == NULL) {
            return false;
        }
        if (currentCoord == &gGfxViewCoord) {
            point->vx = parentPoint.vx;
            point->vy = parentPoint.vy;
            point->vz = parentPoint.vz;
            return true;
        }
        // Keep the halfword truncation at each parent instead of composing matrices.
        ACTOR_RENDER_TRANSFORM_POINT_TO_PARENT(&currentCoord->coord, parentPoint);
        currentCoord = currentCoord->parent;
    }
}
#undef ACTOR_RENDER_TRANSFORM_POINT_TO_PARENT
