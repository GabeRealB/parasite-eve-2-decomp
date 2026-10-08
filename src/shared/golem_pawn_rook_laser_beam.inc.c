/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Scratch-stack block the laser beam is drawn from.
///
/// The beam is walked in eight steps from its near end, each point projected
/// in turn, and a segment is drawn between each point and the one before it.
/// A segment has three pairs of vertices: its start and end on the centre
/// line, and the same two points set off to either side by a width that
/// shrinks with the segment's depth. Reserved for the length of the call.
typedef struct {
    VECTOR  span;       // screen vector from the segment's start to its end; z is zero
    SVECTOR point;      // the beam point being projected, in root space; then `span` normalised (0x1000 for 1), with y negated, whose components, swapped, give the offsets across the beam
    SVECTOR step;       // an eighth of the beam, in root space
    s32     prevScreen; // screen position of the segment's start, x in the low halfword and y in the high
    s32     screen;     // screen position of the segment's end, packed the same way
    s32     prevDepth;  // ordering depth of the segment's start, a quarter of its screen z
    s32     depth;      // ordering depth of the segment's end
    s16     vertexX[6]; // screen x of the segment's vertices: [0], [1] its start and end on the centre line; [2], [3] and [4], [5] the same points on either edge
    s16     vertexY[6]; // screen y of the same vertices
} _GolemPawnRookLaserBeamScratch;
STATIC_ASSERT_SIZEOF(_GolemPawnRookLaserBeamScratch, 0x48);

/// Draws a red additive laser sight between two borrowed body-root-space endpoints.
///
/// nearEnd starts the beam; farEnd defines its span. The body's root workm must
/// already be composed for the active view. Seven segments cover seven eighths
/// of the span, with integer division truncating each step. Segments whose mean
/// ordering depth is below 30 are skipped. Each visible segment takes two shaded
/// quads, a centre line and a draw-mode packet from the current primitive arena;
/// the caller must leave room for up to seven such groups. Scratch is call-owned.
static void _golemPawnRookDrawLaserBeam(Task* actor, const SVECTOR* farEnd, const SVECTOR* nearEnd)
{
    enum {
        GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS         = 8,
        GOLEM_PAWN_ROOK_LASER_MIN_DEPTH            = 30,
        GOLEM_PAWN_ROOK_LASER_WIDTH                = 0x900,
        GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS = 12,
        GOLEM_PAWN_ROOK_LASER_DRAW_MODE            = 0xE1000620, // Draw-to-display, dithering and additive blend.
    };
    _GolemPawnRookLaserBeamScratch* scratch;
    GfxCoord*                       root;
    POLY_G4*                        ribbon;
    LINE_F2*                        line;
    DR_TPAGE*                       drawMode;
    s32                             pointIndex;
    s32                             ribbonIndex;
    s32                             segmentDepth;

    /// Projects a root-space point to packed screen XY and quarter-Z ordering depth.
    ///
    /// root is already composed for this view; point is a borrowed SVECTOR pointer.
    /// screen/depth are writable s32 lvalues. Arguments must be side-effect-free,
    /// including through the GTE macros. Captures no locals, changes GTE state and
    /// expands to a compound statement; undefined below this handler.
#define GOLEM_PAWN_ROOK_PROJECT_LASER_POINT(root, point, screen, depth) \
    {                                                                   \
        gte_SetRotMatrix(&(root)->workm);                               \
        gte_SetTransMatrix(&(root)->workm);                             \
        gte_ldv0((point));                                              \
        gte_rtps();                                                     \
        gte_stsxy(&(screen));                                           \
        gte_stszotz(&(depth));                                          \
    }

    scratch          = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookLaserBeamScratch);
    root             = actor->extra.tmd->coords;
    scratch->step.vx = (farEnd->vx - nearEnd->vx) / GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS;
    scratch->step.vy = (farEnd->vy - nearEnd->vy) / GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS;
    scratch->step.vz = (farEnd->vz - nearEnd->vz) / GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS;
    GOLEM_PAWN_ROOK_PROJECT_LASER_POINT(root, nearEnd, scratch->prevScreen, scratch->prevDepth);
    // Project seven adjacent segments; the final eighth remains undrawn.
    for (pointIndex = 1; pointIndex < GOLEM_PAWN_ROOK_LASER_SUBDIVISIONS; pointIndex++) {
        scratch->point.vx = nearEnd->vx + scratch->step.vx * pointIndex;
        scratch->point.vy = nearEnd->vy + scratch->step.vy * pointIndex;
        scratch->point.vz = nearEnd->vz + scratch->step.vz * pointIndex;
        GOLEM_PAWN_ROOK_PROJECT_LASER_POINT(root, &scratch->point, scratch->screen, scratch->depth);
        segmentDepth = (scratch->prevDepth + scratch->depth) / 2;
        if (segmentDepth < GOLEM_PAWN_ROOK_LASER_MIN_DEPTH) {
            scratch->prevScreen = scratch->screen;
            scratch->prevDepth  = scratch->depth;
            continue;
        }
        // Build two fading ribbons around the projected centre line.
        scratch->span.vz    = 0;
        scratch->vertexX[0] = scratch->prevScreen;
        scratch->vertexY[0] = scratch->prevScreen >> 16;
        scratch->vertexX[1] = scratch->screen;
        scratch->vertexY[1] = scratch->screen >> 16;
        scratch->span.vx    = scratch->vertexX[1] - scratch->vertexX[0];
        scratch->span.vy    = scratch->vertexY[1] - scratch->vertexY[0];
        VectorNormalS(&scratch->span, &scratch->point);
        scratch->point.vy  *= -1;
        scratch->vertexX[2] = scratch->vertexX[0] + (-(scratch->point.vy * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexX[3] = scratch->vertexX[1] + (-(scratch->point.vy * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexX[4] = scratch->vertexX[0] + ((scratch->point.vy * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        scratch->vertexY[2] = scratch->vertexY[0] + (-(scratch->point.vx * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexY[3] = scratch->vertexY[1] + (-(scratch->point.vx * GOLEM_PAWN_ROOK_LASER_WIDTH) >> GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS) / segmentDepth;
        scratch->vertexY[4] = scratch->vertexY[0] + ((scratch->point.vx * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        scratch->vertexY[5] = scratch->vertexY[1] + ((scratch->point.vx * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        scratch->vertexX[5] = scratch->vertexX[1] + ((scratch->point.vy * (GOLEM_PAWN_ROOK_LASER_WIDTH >> 8)) >> (GOLEM_PAWN_ROOK_LASER_NORMAL_FRACTION_BITS - 8)) / segmentDepth;
        for (ribbonIndex = 0; ribbonIndex < ARRAY_SIZE(gGolemPawnRookBeamRibbonCorners); ribbonIndex++) {
            ribbon         = gGpuPrimCursor;
            gGpuPrimCursor = ribbon + 1;
            setPolyG4(ribbon);
            setSemiTrans(ribbon, 1);
            ribbon->x0 = scratch->vertexX[gGolemPawnRookBeamRibbonCorners[ribbonIndex][0]];
            ribbon->y0 = scratch->vertexY[gGolemPawnRookBeamRibbonCorners[ribbonIndex][0]];
            ribbon->x1 = scratch->vertexX[gGolemPawnRookBeamRibbonCorners[ribbonIndex][1]];
            ribbon->y1 = scratch->vertexY[gGolemPawnRookBeamRibbonCorners[ribbonIndex][1]];
            ribbon->x2 = scratch->vertexX[gGolemPawnRookBeamRibbonCorners[ribbonIndex][2]];
            ribbon->y2 = scratch->vertexY[gGolemPawnRookBeamRibbonCorners[ribbonIndex][2]];
            ribbon->x3 = scratch->vertexX[gGolemPawnRookBeamRibbonCorners[ribbonIndex][3]];
            ribbon->y3 = scratch->vertexY[gGolemPawnRookBeamRibbonCorners[ribbonIndex][3]];
            setRGB0(ribbon, 0xFF, 0, 0);
            setRGB1(ribbon, 0xFF, 0, 0);
            setRGB2(ribbon, 0, 0, 0);
            setRGB3(ribbon, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(segmentDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), ribbon);
        }
        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setSemiTrans(line, 1);
        line->x0 = scratch->prevScreen;
        line->y0 = scratch->prevScreen >> 16;
        line->x1 = scratch->screen;
        line->y1 = scratch->screen >> 16;
        setRGB0(line, 0xFF, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(segmentDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), line);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = GOLEM_PAWN_ROOK_LASER_DRAW_MODE;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(segmentDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), drawMode);
        scratch->prevScreen = scratch->screen;
        scratch->prevDepth  = scratch->depth;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserBeamScratch);

#undef GOLEM_PAWN_ROOK_PROJECT_LASER_POINT
}
