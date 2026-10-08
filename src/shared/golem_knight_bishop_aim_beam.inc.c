/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block the aim beam is drawn from.
///
/// The beam is cut into eight segments between its two projected ends. Each
/// segment has three pairs of vertices: its start and end on the centre line,
/// and the same two points set off to either side by a width that shrinks
/// with the segment's depth. Reserved for the length of the call.
typedef struct {
    VECTOR  span;       // screen vector from the beam's far end to the point on part 4; z is zero
    SVECTOR direction;  // `span` normalised (0x1000 for 1), with y negated; its components, swapped, give the offsets across the beam
    s32     depth;      // ordering depth at the end of the segment being drawn; the segment is skipped below 0x1E
    s32     depthStep;  // an eighth of the depth difference between the beam's ends
    u16     vertexX[6]; // screen x of the segment's vertices: [0], [1] its start and end on the centre line; [2], [3] and [4], [5] the same points on either edge
    u16     vertexY[6]; // screen y of the same vertices
    s16     stepX;      // an eighth of the beam's extent in screen x: one segment's length
    s16     stepY;      // the same in screen y
} _GolemKnightBishopAimBeamScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopAimBeamScratch, 0x3C);

/// Queues the red core and two ribbons that fade to black across one beam segment.
///
/// `beam` supplies six screen-pixel vertex pairs and a positive quarter-Z depth.
/// The carrier's two corner rows select vertices 0..5. The current GPU arena
/// must have room for two `POLY_G4`, one `LINE_F2` and one `DR_TPAGE`, and remain
/// live through submission; the scratch input is only borrowed during this call.
static inline void _golemKnightBishopEmitAimBeamSegment(const _GolemKnightBishopAimBeamScratch* beam)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BEAM_DRAW_MODE   = 0xE1000620, // Additive blending, dithering and drawing in the display area.
        GOLEM_KNIGHT_BISHOP_BEAM_CORE_CODE   = 0x42,
        GOLEM_KNIGHT_BISHOP_BEAM_RIBBON_CODE = 0x3A,
    };
    POLY_G4*  ribbon;
    LINE_F2*  line;
    DR_TPAGE* drawMode;
    s32       ribbonIndex;
    for (ribbonIndex = 0; ribbonIndex < ARRAY_SIZE(gGolemKnightBishopBeamQuadCorners); ribbonIndex++) {
        ribbon         = gGpuPrimCursor;
        gGpuPrimCursor = ribbon + 1;
        setlen(ribbon, sizeof(*ribbon) / sizeof(u32) - 1);
        ribbon->code = GOLEM_KNIGHT_BISHOP_BEAM_RIBBON_CODE;
        ribbon->x0   = beam->vertexX[gGolemKnightBishopBeamQuadCorners[ribbonIndex][0]];
        ribbon->y0   = beam->vertexY[gGolemKnightBishopBeamQuadCorners[ribbonIndex][0]];
        ribbon->x1   = beam->vertexX[gGolemKnightBishopBeamQuadCorners[ribbonIndex][1]];
        ribbon->y1   = beam->vertexY[gGolemKnightBishopBeamQuadCorners[ribbonIndex][1]];
        ribbon->x2   = beam->vertexX[gGolemKnightBishopBeamQuadCorners[ribbonIndex][2]];
        ribbon->y2   = beam->vertexY[gGolemKnightBishopBeamQuadCorners[ribbonIndex][2]];
        ribbon->x3   = beam->vertexX[gGolemKnightBishopBeamQuadCorners[ribbonIndex][3]];
        ribbon->y3   = beam->vertexY[gGolemKnightBishopBeamQuadCorners[ribbonIndex][3]];
        setRGB0(ribbon, 0xFF, 0, 0);
        setRGB1(ribbon, 0xFF, 0, 0);
        setRGB2(ribbon, 0, 0, 0);
        setRGB3(ribbon, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(beam->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), ribbon);
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setlen(line, sizeof(*line) / sizeof(u32) - 1);
    line->code = GOLEM_KNIGHT_BISHOP_BEAM_CORE_CODE;
    line->x0   = beam->vertexX[0];
    line->y0   = beam->vertexY[0];
    line->x1   = beam->vertexX[1];
    line->y1   = beam->vertexY[1];
    setRGB0(line, 0xFF, 0, 0);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(beam->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), line);
    // Prepending the draw mode last makes it execute before the additive packets.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, sizeof(*drawMode) / sizeof(u32) - 1);
    drawMode->code[0] = GOLEM_KNIGHT_BISHOP_BEAM_DRAW_MODE;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((u32)(beam->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK), drawMode);
}

/// Draws the red aim beam from the work block's projected endpoints.
///
/// Requires the screen X/Y and quarter-Z depths prepared by
/// `_golemKnightBishopAimFromPart`. Eight screen-space segments use their ending
/// depth for both width and ordering; depths below 30 are skipped. Each segment
/// queues two fading additive ribbons and a red core into the current GPU arena.
/// The arena must have room for the packets and stay live through GPU submission.
static void _golemKnightBishopDrawAimBeam(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS  = 8,
        GOLEM_KNIGHT_BISHOP_BEAM_MIN_DEPTH = 30,
    };
    _GolemKnightBishopAimBeamScratch* beam;
    GolemKnightBishopWork*            work;
    s32                               segmentIndex;

    beam          = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopAimBeamScratch);
    work          = task->work;
    beam->span.vx = work->beamScreenX[1] - work->beamScreenX[0];
    beam->span.vy = work->beamScreenY[1] - work->beamScreenY[0];
    beam->span.vz = 0;
    VectorNormalS(&beam->span, &beam->direction);
    beam->direction.vy *= -1;
    beam->stepX         = (work->beamScreenX[1] - work->beamScreenX[0]) / GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS;
    beam->stepY         = (work->beamScreenY[1] - work->beamScreenY[0]) / GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS;
    beam->depthStep     = (work->beamDepth[1] - work->beamDepth[0]) / GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS;
    // Interpolate in screen space; width and ordering use the segment end depth.
    for (segmentIndex = 0; segmentIndex < GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS; segmentIndex++) {
        beam->depth = beam->depthStep * (segmentIndex + 1) + work->beamDepth[0];
        if (beam->depth < GOLEM_KNIGHT_BISHOP_BEAM_MIN_DEPTH) {
            continue;
        }
        beam->vertexX[0] = work->beamScreenX[0] + beam->stepX * segmentIndex;
        beam->vertexX[1] = work->beamScreenX[0] + beam->stepX * (segmentIndex + 1);
        beam->vertexX[2] = beam->vertexX[0] + ((-(beam->direction.vy * 0x600) >> 12) / beam->depth);
        beam->vertexX[3] = beam->vertexX[1] + ((-(beam->direction.vy * 0x600) >> 12) / beam->depth);
        beam->vertexX[4] = beam->vertexX[0] + (((beam->direction.vy * 3) >> 3) / beam->depth);
        beam->vertexX[5] = beam->vertexX[1] + (((beam->direction.vy * 3) >> 3) / beam->depth);
        beam->vertexY[0] = work->beamScreenY[0] + beam->stepY * segmentIndex;
        beam->vertexY[1] = work->beamScreenY[0] + beam->stepY * (segmentIndex + 1);
        beam->vertexY[2] = beam->vertexY[0] + ((-(beam->direction.vx * 0x600) >> 12) / beam->depth);
        beam->vertexY[3] = beam->vertexY[1] + ((-(beam->direction.vx * 0x600) >> 12) / beam->depth);
        beam->vertexY[4] = beam->vertexY[0] + (((beam->direction.vx * 3) >> 3) / beam->depth);
        beam->vertexY[5] = beam->vertexY[1] + (((beam->direction.vx * 3) >> 3) / beam->depth);
        _golemKnightBishopEmitAimBeamSegment(beam);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopAimBeamScratch);
}
