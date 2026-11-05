/* Part of the water effects library; see water.h. */

// Each include defines one strip drawer, configured by the macros below, which it undefines at its end. Required:
// WATER_WAVE_STRIPS_SURFACES (the surface table), WATER_WAVE_STRIPS_HEIGHT (its world Y),
// WATER_WAVE_STRIPS_PACKET_CURSOR (the `u8*` lvalue packets are written at) and WATER_WAVE_STRIPS_AMPLITUDE_SHIFT
// (0..16), which sets the wave's height: 5 gives -128..128 units (septic tank), 6 gives -64..64 (main corridor, water
// supply).
#ifndef WATER_WAVE_STRIPS_AMPLITUDE_SHIFT
#error "Define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT before including wave_strips.inc.c"
#elif WATER_WAVE_STRIPS_AMPLITUDE_SHIFT < 0 || WATER_WAVE_STRIPS_AMPLITUDE_SHIFT > 16
#error "WATER_WAVE_STRIPS_AMPLITUDE_SHIFT must be in 0..16 to preserve signed Y offsets"
#endif

#ifndef WATER_WAVE_STRIPS_DRAW_FUNCTION
/// Name of the drawer this include defines; the septic tank names its second one.
#define WATER_WAVE_STRIPS_DRAW_FUNCTION _waterDrawWaveStrips
#endif
#ifndef WATER_WAVE_STRIPS_SURFACE_T
/// Type of the surface table's entries (`x`, `z`, `width`, `depth`); water supply binds `RoomCompactWaterSurface` with
/// its own `WATER_WAVE_STRIPS_IS_LIST_END`.
#define WATER_WAVE_STRIPS_SURFACE_T RoomWaterSurface
#endif
#ifndef WATER_WAVE_STRIPS_IS_LIST_END
/// Whether `surface` is the table's terminator.
#define WATER_WAVE_STRIPS_IS_LIST_END(surface) ((surface)->segmentCount == WATER_SURFACE_LIST_END)
#endif
#ifndef WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
/// 1 to point the packet cursor at the free actor-load buffer half for this display buffer (main corridor, which also
/// skips views 10 and 11); 0, the default, appends at the cursor the caller set.
#define WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR 0
#endif
#ifndef WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS
#ifndef WATER_WAVE_STRIPS_FIRST_COLOURS_HELPER_DEFINED
/// Marks the default first-strip color helper as defined, so a later include reuses it.
#define WATER_WAVE_STRIPS_FIRST_COLOURS_HELPER_DEFINED
/// Default first-strip colors: (128, 0, 0) at the flat edge (vertices 0/1), (32, 32, 32) at the waving seam (2/3).
static inline void _waterSetFirstWaveStripColours(POLY_G4* quad)
{
    quad->r0 = 0x80;
    quad->r1 = 0x80;
    quad->g0 = 0;
    quad->b0 = 0;
    quad->g1 = 0;
    quad->b1 = 0;
    quad->r2 = 0x20;
    quad->g2 = 0x20;
    quad->b2 = 0x20;
    quad->r3 = 0x20;
    quad->g3 = 0x20;
    quad->b3 = 0x20;
}
#endif

/// Sets a first-strip quad's vertex colors, 0/1 at the flat edge and 2/3 at the seam; the septic tank binds its blue
/// palette.
#define WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad) _waterSetFirstWaveStripColours(quad)
#endif
#ifndef WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS
#ifndef WATER_WAVE_STRIPS_SECOND_COLOURS_HELPER_DEFINED
/// Marks the default second-strip color helper as defined, so a later include reuses it.
#define WATER_WAVE_STRIPS_SECOND_COLOURS_HELPER_DEFINED
/// Default second-strip colors: (32, 32, 32) at the waving seam (vertices 0/1), (128, 0, 0) at the flat edge (2/3).
static inline void _waterSetSecondWaveStripColours(POLY_G4* quad)
{
    quad->r2 = 0x80;
    quad->r3 = 0x80;
    quad->g2 = 0;
    quad->b2 = 0;
    quad->g3 = 0;
    quad->b3 = 0;
    quad->r0 = 0x20;
    quad->g0 = 0x20;
    quad->b0 = 0x20;
    quad->r1 = 0x20;
    quad->g1 = 0x20;
    quad->b1 = 0x20;
}
#endif

/// Sets a second-strip quad's vertex colors, 0/1 at the seam and 2/3 at the flat edge; the septic tank binds its blue
/// palette.
#define WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad) _waterSetSecondWaveStripColours(quad)
#endif

/// Draws each surface rectangle as two subtractive strips of sixteen quads, meeting at a seam halfway across X whose
/// height waves along Z and scrolls each frame. `task` is unused.
static void WATER_WAVE_STRIPS_DRAW_FUNCTION(Task* task)
{
    enum {
        WATER_WAVE_STRIPS_SEGMENTS_PER_STRIP = 16,
        WATER_WAVE_STRIPS_PHASE_STEP_SHIFT   = 9,  // 512 angle units, one eighth-turn
        WATER_WAVE_STRIPS_PHASE_PER_FRAME    = 16, // Angle units per display frame
        WATER_WAVE_STRIPS_PACKET_HALF_BYTES  = 0xC000,
        WATER_WAVE_STRIPS_FIRST_HIDDEN_VIEW  = 10,
        WATER_WAVE_STRIPS_HIDDEN_VIEW_COUNT  = 2
    };
    SVECTOR                            vertex0, vertex1, vertex2, vertex3;
    long                               screenXY0, screenXY1, screenXY2, screenXY3;
    long                               projectionScale, projectionFlags;
    s32                                wavePhase;
    const WATER_WAVE_STRIPS_SURFACE_T* surface;
    WaterQuadScratch*                  strip;
    WaterQuadScratch*                  scratchTop;
    POLY_G4*                           quad;
    DR_MODE*                           drawMode;
    s32                                depth;
    s32                                segment;
#if WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
    GameLocationKey* location;
#endif

    /// Sorts the finished quad at `depth`, preceded by a draw-mode packet that sets subtractive blending.
#define WATER_WAVE_STRIPS_QUEUE_QUAD()                                           \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth), quad);                     \
    drawMode                        = (DR_MODE*)WATER_WAVE_STRIPS_PACKET_CURSOR; \
    WATER_WAVE_STRIPS_PACKET_CURSOR = (u8*)(drawMode + 1);                       \
    setDrawTPage(drawMode, 0, 0, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));       \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth), drawMode)

#if WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
    surface   = WATER_WAVE_STRIPS_SURFACES;
    wavePhase = -(gDisplayState.animFrame * WATER_WAVE_STRIPS_PHASE_PER_FRAME);
    location  = &gGameSession->location;
    // These corridor views omit water, before reserving either packet or scratch space.
    if (location->stage == GAME_STAGE_MINE_SHELTER) {
        if (location->area == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
            if ((u32)(gGameSession->location.view - WATER_WAVE_STRIPS_FIRST_HIDDEN_VIEW) < WATER_WAVE_STRIPS_HIDDEN_VIEW_COUNT) {
                return;
            }
        }
    }
    // Borrow the unused actor-load area, split by the display's buffer index.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        WATER_WAVE_STRIPS_PACKET_CURSOR = gFsActorBuffer2Base + gDisplayState.otBuffer * WATER_WAVE_STRIPS_PACKET_HALF_BYTES;
    } else {
        WATER_WAVE_STRIPS_PACKET_CURSOR = gFsActorBuffer1Base + gDisplayState.otBuffer * WATER_WAVE_STRIPS_PACKET_HALF_BYTES;
    }
    scratchTop                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
#else
    surface                    = WATER_WAVE_STRIPS_SURFACES;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchTop                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    wavePhase                  = -(gDisplayState.animFrame * WATER_WAVE_STRIPS_PHASE_PER_FRAME);
#endif
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchTop - 1;
    strip                                  = scratchTop - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    strip->y = WATER_WAVE_STRIPS_HEIGHT;
    for (; !WATER_WAVE_STRIPS_IS_LIST_END(surface); surface++) {
        strip->dx = surface->width / 2;
        strip->dz = surface->depth / WATER_WAVE_STRIPS_SEGMENTS_PER_STRIP;
        strip->x  = surface->x;
        strip->z  = surface->z;
        // First strip: flat outer edge to the displaced seam.
        for (segment = 0; segment < WATER_WAVE_STRIPS_SEGMENTS_PER_STRIP; segment++) {
            vertex0.vx     = strip->x;
            vertex0.vy     = strip->y;
            vertex0.vz     = strip->z + strip->dz * segment;
            vertex1.vx     = strip->x;
            vertex1.vy     = strip->y;
            vertex1.vz     = strip->z + strip->dz * (segment + 1);
            strip->yOffset = (u32)rsin(wavePhase + (segment << WATER_WAVE_STRIPS_PHASE_STEP_SHIFT)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            vertex2.vx     = strip->x + strip->dx;
            vertex2.vy     = strip->y + strip->yOffset;
            vertex2.vz     = strip->z + strip->dz * segment;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << WATER_WAVE_STRIPS_PHASE_STEP_SHIFT)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            vertex3.vx     = strip->x + strip->dx;
            vertex3.vy     = strip->y + strip->yOffset;
            vertex3.vz     = strip->z + strip->dz * (segment + 1);
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                            = (POLY_G4*)WATER_WAVE_STRIPS_PACKET_CURSOR;
                WATER_WAVE_STRIPS_PACKET_CURSOR = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad);
                WATER_WAVE_STRIPS_QUEUE_QUAD();
            }
        }
        // Second strip: the same displaced seam to the other flat outer edge.
        for (segment = 0; segment < WATER_WAVE_STRIPS_SEGMENTS_PER_STRIP; segment++) {
            strip->yOffset = (u32)rsin(wavePhase + (segment << WATER_WAVE_STRIPS_PHASE_STEP_SHIFT)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            vertex0.vx     = strip->x + strip->dx;
            vertex0.vy     = strip->y + strip->yOffset;
            vertex0.vz     = strip->z + strip->dz * segment;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << WATER_WAVE_STRIPS_PHASE_STEP_SHIFT)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            vertex1.vx     = strip->x + strip->dx;
            vertex1.vy     = strip->y + strip->yOffset;
            vertex1.vz     = strip->z + strip->dz * (segment + 1);
            vertex2.vx     = strip->x + strip->dx * 2;
            vertex2.vy     = strip->y;
            vertex2.vz     = strip->z + strip->dz * segment;
            vertex3.vx     = strip->x + strip->dx * 2;
            vertex3.vy     = strip->y;
            vertex3.vz     = strip->z + strip->dz * (segment + 1);
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                            = (POLY_G4*)WATER_WAVE_STRIPS_PACKET_CURSOR;
                WATER_WAVE_STRIPS_PACKET_CURSOR = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad);
                WATER_WAVE_STRIPS_QUEUE_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
#undef WATER_WAVE_STRIPS_QUEUE_QUAD
}

#undef WATER_WAVE_STRIPS_DRAW_FUNCTION
#undef WATER_WAVE_STRIPS_SURFACES
#undef WATER_WAVE_STRIPS_HEIGHT
#undef WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS
#undef WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS
#undef WATER_WAVE_STRIPS_SURFACE_T
#undef WATER_WAVE_STRIPS_IS_LIST_END
#undef WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
#undef WATER_WAVE_STRIPS_AMPLITUDE_SHIFT
#undef WATER_WAVE_STRIPS_PACKET_CURSOR
