/* Part of the water hole library; see water_hole.h. */

/// Draws the water-hole surfaces as two subtractive strips joined at a waving seam.
///
/// Each descriptor supplies world XYZ and extents. Width / 64 and depth / 2
/// truncate to whole units; all generated vertices narrow to signed halfwords.
/// The seam's sine has a 16-unit amplitude, advances an eighth-turn per column
/// and scrolls -16 angle units per running-actor tick (4096 units per turn).
/// A negative GTE projection flag rejects the quad; accepted quads consume
/// sizeof(POLY_G4) + sizeof(DR_MODE) = 48 arena bytes, up to 0x1800 per surface.
/// Only the first draw-mode word is sent. The surface list ends at Y = -1.
///
/// Resets the current 0xC000-byte half of the unused actor arena (2 without a
/// companion, otherwise 1). That area and the ordering table must stay live
/// through GPU drawing, with enough room for the complete terminated list.
/// The task argument is unused and permits state-table dispatch.
static void _waterHoleDrawSurfaces(Task* task)
{
    enum { WATER_HOLE_COLUMNS_PER_STRIP  = 64,
           WATER_HOLE_PACKET_HALF_BYTES  = 0xC000,
           WATER_HOLE_PHASE_PER_TICK     = 16,
           WATER_HOLE_COLUMN_PHASE_SHIFT = 9,
           WATER_HOLE_WAVE_AMPLITUDE     = 16,
           WATER_HOLE_SINE_FRACTION_BITS = 12 };
    SVECTOR           vertex0, vertex1, vertex2, vertex3;
    long              screenXY0, screenXY1, screenXY2, screenXY3;
    long              projectionScale, projectionFlags;
    s32               columnWidth;
    s32               wavePhase;
    WaterHoleSurface* surface;
    POLY_G4*          quad;
    DR_MODE*          drawMode;
    s32               depth;
    s32               columnIndex;
    s32               halfDepth;
    s32               waveHeight;

    /// Queues the completed quad and its subtractive mode in the same depth bucket.
    ///
    /// Captures quad, depth, drawMode, the byte cursor and current display table.
    /// Takes no arguments; requires a braced call site and arena room for a full
    /// DR_MODE. Prepending the mode last makes it execute before the quad.
#define WATER_HOLE_QUEUE_QUAD()                                                                                                                \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            quad);                                                                                                                             \
    drawMode             = (DR_MODE*)gWaterHolePrimCursor;                                                                                     \
    gWaterHolePrimCursor = (u8*)(drawMode + 1);                                                                                                \
    setDrawTPage(drawMode, 0, 0, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));                                                                     \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            drawMode);

    surface = gWaterHoleSurfaces;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gWaterHolePrimCursor = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * WATER_HOLE_PACKET_HALF_BYTES;
    } else {
        gWaterHolePrimCursor = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * WATER_HOLE_PACKET_HALF_BYTES;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        gWaterHoleWaveScroll++;
    }
    wavePhase                  = -(gWaterHoleWaveScroll * WATER_HOLE_PHASE_PER_TICK);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    // Both Z strips share the same waving X seam and retain flat outer edges.
    for (; surface->y != WATER_SURFACE_LIST_END; surface++) {
        columnWidth = surface->width / WATER_HOLE_COLUMNS_PER_STRIP;
        halfDepth   = surface->depth / 2;
        for (columnIndex = 0; columnIndex < WATER_HOLE_COLUMNS_PER_STRIP; columnIndex++) {
            vertex0.vx = surface->x + columnWidth * columnIndex;
            vertex0.vy = surface->y;
            vertex0.vz = surface->z;
            vertex1.vx = surface->x + columnWidth * (columnIndex + 1);
            vertex1.vy = surface->y;
            vertex1.vz = surface->z;
            waveHeight = (rsin(wavePhase + (columnIndex << WATER_HOLE_COLUMN_PHASE_SHIFT)) * WATER_HOLE_WAVE_AMPLITUDE) >> WATER_HOLE_SINE_FRACTION_BITS;
            vertex2.vx = surface->x + columnWidth * columnIndex;
            vertex2.vy = surface->y + waveHeight;
            vertex2.vz = surface->z + halfDepth;
            waveHeight = (rsin(wavePhase + ((columnIndex + 1) << WATER_HOLE_COLUMN_PHASE_SHIFT)) * WATER_HOLE_WAVE_AMPLITUDE) >> WATER_HOLE_SINE_FRACTION_BITS;
            vertex3.vx = surface->x + columnWidth * (columnIndex + 1);
            vertex3.vy = surface->y + waveHeight;
            vertex3.vz = surface->z + halfDepth;
            depth      = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                 = (POLY_G4*)gWaterHolePrimCursor;
                gWaterHolePrimCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r0                       = 0xFF;
                quad->r1                       = 0xFF;
                quad->g0                       = 0;
                quad->b0                       = 0;
                quad->g1                       = 0;
                quad->b1                       = 0;
                quad->r2                       = 0x20;
                quad->g2                       = 0x20;
                quad->b2                       = 0x20;
                quad->r3                       = 0x20;
                quad->g3                       = 0x20;
                quad->b3                       = 0x20;
                WATER_HOLE_QUEUE_QUAD();
            }
        }
        for (columnIndex = 0; columnIndex < WATER_HOLE_COLUMNS_PER_STRIP; columnIndex++) {
            waveHeight = (rsin(wavePhase + (columnIndex << WATER_HOLE_COLUMN_PHASE_SHIFT)) * WATER_HOLE_WAVE_AMPLITUDE) >> WATER_HOLE_SINE_FRACTION_BITS;
            vertex0.vx = surface->x + columnWidth * columnIndex;
            vertex0.vy = surface->y + waveHeight;
            vertex0.vz = surface->z + halfDepth;
            waveHeight = (rsin(wavePhase + ((columnIndex + 1) << WATER_HOLE_COLUMN_PHASE_SHIFT)) * WATER_HOLE_WAVE_AMPLITUDE) >> WATER_HOLE_SINE_FRACTION_BITS;
            vertex1.vx = surface->x + columnWidth * (columnIndex + 1);
            vertex1.vy = surface->y + waveHeight;
            vertex1.vz = surface->z + halfDepth;
            vertex2.vx = surface->x + columnWidth * columnIndex;
            vertex2.vy = surface->y;
            vertex2.vz = surface->z + halfDepth * 2;
            vertex3.vx = surface->x + columnWidth * (columnIndex + 1);
            vertex3.vy = surface->y;
            vertex3.vz = surface->z + halfDepth * 2;
            depth      = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                 = (POLY_G4*)gWaterHolePrimCursor;
                gWaterHolePrimCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r2                       = 0xFF;
                quad->r3                       = 0xFF;
                quad->g2                       = 0;
                quad->b2                       = 0;
                quad->g3                       = 0;
                quad->b3                       = 0;
                quad->r0                       = 0x20;
                quad->g0                       = 0x20;
                quad->b0                       = 0x20;
                quad->r1                       = 0x20;
                quad->g1                       = 0x20;
                quad->b1                       = 0x20;
                WATER_HOLE_QUEUE_QUAD();
            }
        }
    }
#undef WATER_HOLE_QUEUE_QUAD
}
