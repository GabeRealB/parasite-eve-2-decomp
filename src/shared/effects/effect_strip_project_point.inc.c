/* Shared by the Antibody and Apobiosis effects. */

/// Projects the selected strip endpoint through the loaded GTE matrices.
///
/// Borrows one complete word-aligned scratch block. `projectEnd` selects
/// worldEnd/screenEnd when true and worldStart/screenStart otherwise. Writes
/// signed screen pixels and replaces projectionFlags. Leaves SZ3 available
/// for the caller to capture before the next projection.
static inline void _effectStripProjectPoint(EffectStripScratch* scratch, bool projectEnd)
{
    if (projectEnd) {
        gte_ldv0(&scratch->worldEnd);
        gte_rtps();
        gte_stsxy(&scratch->screenEnd);
        gte_stflg(&scratch->projectionFlags);
    } else {
        gte_ldv0(&scratch->worldStart);
        gte_rtps();
        gte_stsxy(&scratch->screenStart);
        gte_stflg(&scratch->projectionFlags);
    }
}
