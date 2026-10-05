/* Part of the water effects library; see water_effects.h. */

// WATER_WAVE_STRIPS_AMPLITUDE_SHIFT is required for each included instance.
// Bind an integer preprocessor constant in 0..16: it scales the signed
// -4096..4096 sine result into world-coordinate Y displacement at the seam.
// The septic tank binds 5 (-128..128 units); the main corridor and water supply
// bind 6 (-64..64 units). The unsigned 32-bit shift followed by the signed
// 16-bit yOffset store preserves negative offsets throughout this shift range.
// The binding captures no identifiers, has no side effects, and is undefined
// at the end of this file; a second include must bind it again.
#ifndef WATER_WAVE_STRIPS_AMPLITUDE_SHIFT
#error "Define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT before including water_wave_strips.inc.c"
#elif WATER_WAVE_STRIPS_AMPLITUDE_SHIFT < 0 || WATER_WAVE_STRIPS_AMPLITUDE_SHIFT > 16
#error "WATER_WAVE_STRIPS_AMPLITUDE_SHIFT must be in 0..16 to preserve signed Y offsets"
#endif

#ifndef WATER_WAVE_STRIPS_DRAW_FUNCTION
/// Selects the private task-compatible function emitted by this include.
///
/// Bind a single function identifier for a `static void (Task*)` definition.
/// The default serves one list per carrier; the septic tank overrides it for
/// its second list. The task argument is unused. There are no captured locals,
/// evaluations, stringification or token pasting; the include undefines the
/// binding, so a later instance must bind its distinct identifier again.
#define WATER_WAVE_STRIPS_DRAW_FUNCTION _waterDrawWaveStrips
#endif
#ifndef WATER_WAVE_STRIPS_SURFACE_T
#define WATER_WAVE_STRIPS_SURFACE_T RoomWaterSurface
#endif
#ifndef WATER_WAVE_STRIPS_IS_LIST_END
/// Tests whether a descriptor terminates the fixed water-strip surface list.
///
/// Returns an int (0 drawable entry, 1 list end). `surface` must point to a
/// readable `WATER_WAVE_STRIPS_SURFACE_T` descriptor in a table terminated
/// within its bounds. Only the signed marker is read, before any geometry;
/// other marker values, including 0, do not affect the fixed 16 subdivisions.
/// The main corridor and both septic-tank instances use this default. An
/// alternate surface type must bind a matching predicate before the include.
/// Definitions must evaluate the pointer once and preserve the signed
/// `WATER_SURFACE_LIST_END` comparison, with no additional side effects or
/// captured caller locals. This include undefines the binding; each instance
/// must supply its override again.
#define WATER_WAVE_STRIPS_IS_LIST_END(surface) ((surface)->segmentCount == WATER_SURFACE_LIST_END)
#endif
#ifndef WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
/// Selects whether the strip drawer resets its actor-load packet cursor.
///
/// Define to the integer literal 0 or 1 before including this file. With 0
/// (the default), packets append to the caller-initialized cursor; the septic
/// tank and water supply reset it once before drawing both surface lists.
/// With 1, the main corridor drawer selects actor-load buffer 2 when the live
/// save's companion type is zero, otherwise buffer 1, and resets the cursor to
/// that buffer's current 0xC000-byte half on each drawing call. This prologue
/// also skips stage 4, area 0x21, views 10 and 11, returning before any cursor
/// reset or scratch reservation.
///
/// `WATER_WAVE_STRIPS_PACKET_CURSOR` must bind a writable `u8*` lvalue. The caller
/// must reserve the selected packet area, with word alignment and enough space
/// for the surface lists, until the GPU has consumed it. This configuration
/// binding is tested only by the preprocessor and undefined after each include.
#define WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR 0
#endif
#ifndef WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS
#ifndef WATER_WAVE_STRIPS_FIRST_COLOURS_HELPER_DEFINED
/// Keeps one definition of the default first-strip colour helper per translation unit.
///
/// This include sets an empty presence marker when it supplies the default
/// helper; an override of `WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS` bypasses
/// both. The marker stays defined after each include, while the colour binding
/// is undefined, so later default instances reuse the helper. Includers must
/// leave the marker intact and make Psy-Q's `POLY_G4` visible before the first
/// default instance.
#define WATER_WAVE_STRIPS_FIRST_COLOURS_HELPER_DEFINED
/// Sets the first X strip's default vertex colours for subtractive blending.
///
/// Writes all RGB bytes: (128, 0, 0) on the flat edge (vertices 0/1),
/// (32, 32, 32) on the waving seam (2/3). `quad` is writable; other fields
/// are preserved and the pointer is not retained.
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

/// Sets the first X strip's vertex colours for subtractive blending.
///
/// Vertices 0/1 lie on the flat outer edge at x; 2/3 lie on the waving seam
/// at x + width / 2. RGB components are unsigned bytes. The default uses
/// (0x80, 0, 0) at the outer edge and (0x20, 0x20, 0x20) at the seam; the
/// main corridor and water supply use it. The septic tank binds its blue
/// palette before each include instead. Overrides must be statement macros
/// accepting a writable `POLY_G4*`, setting all twelve RGB bytes and
/// preserving the rest of the packet. The supplied definitions evaluate the
/// argument once, capture no caller locals and retain no packet pointer.
/// The include undefines the binding, so a second instance must supply any
/// override again. The guarded default helper can serve several included
/// instances. Psy-Q's `POLY_G4` must be visible.
#define WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad) _waterSetFirstWaveStripColours(quad)
#endif
#ifndef WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS
#ifndef WATER_WAVE_STRIPS_SECOND_COLOURS_HELPER_DEFINED
/// Keeps one definition of the default second-strip colour helper per translation unit.
///
/// This include sets an empty presence marker when it supplies the default
/// helper; an override of `WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS` bypasses
/// both. The marker stays defined after each include, while the colour binding
/// is undefined, so later default instances reuse the helper. Includers must
/// leave the marker intact and make Psy-Q's `POLY_G4` visible before the first
/// default instance.
#define WATER_WAVE_STRIPS_SECOND_COLOURS_HELPER_DEFINED
/// Sets the second X strip's default vertex colours for subtractive blending.
///
/// Writes all RGB bytes: (32, 32, 32) on the waving seam (vertices 0/1),
/// (128, 0, 0) on the flat edge (2/3). `quad` is writable; other fields
/// are preserved and the pointer is not retained.
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

/// Sets the second X strip's vertex colours for subtractive blending.
///
/// Vertices 0/1 lie on the waving seam; 2/3 lie on the outer edge at
/// x + 2 * (width / 2). RGB components are unsigned bytes. The default uses
/// (0x20, 0x20, 0x20) at the seam and (0x80, 0, 0) at the outer edge; the main
/// corridor and water supply use it. The septic tank binds its blue palette
/// before each include instead. Overrides must be statement macros accepting
/// a writable `POLY_G4*`, setting all twelve RGB bytes and preserving the rest
/// of the packet. The supplied definitions evaluate the argument once and
/// capture no caller locals. The include undefines the binding, so a second
/// instance must supply any override again. The guarded default helper can
/// serve several included instances. Psy-Q's `POLY_G4` must be visible.
#define WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad) _waterSetSecondWaveStripColours(quad)
#endif

/// Draws rectangular water patches as two subtractive wave strips subdivided along Z.
///
/// Each rectangle produces at most 32 quads: sixteen per side of a waving X
/// seam. Width/2 and depth/16 use signed integer division and are narrowed to
/// world-coordinate halfwords. The sine phase advances 512/4096 turn per
/// segment and scrolls -16/4096 turn per display frame. Projection through the
/// view matrix rejects quads with a negative GTE flag word.
///
/// `WATER_WAVE_STRIPS_SURFACES` supplies a readable descriptor table terminated
/// within its bounds; `WATER_WAVE_STRIPS_HEIGHT` supplies signed world Y.
/// `WATER_WAVE_STRIPS_PACKET_CURSOR` supplies a word-aligned byte cursor with
/// 32 * (sizeof(POLY_G4) + sizeof(DR_MODE)) = 0x600 bytes available per drawable
/// rectangle. Each accepted quad advances past both complete packet objects,
/// although the draw-mode packet sends only its first command word. The arena
/// must remain reserved until the GPU has consumed the frame's ordering table.
/// Cursor reset is selected by `WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR`;
/// append instances require the caller to initialize it. Reserves and releases
/// one `WaterQuadScratch`. `task` is unused; the signature permits task dispatch.
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
    SVECTOR                      vertex0, vertex1, vertex2, vertex3;
    long                         screenXY0, screenXY1, screenXY2, screenXY3;
    long                         projectionScale, projectionFlags;
    s32                          wavePhase;
    WATER_WAVE_STRIPS_SURFACE_T* surface;
    WaterQuadScratch*            strip;
    WaterQuadScratch*            scratchTop;
    POLY_G4*                     quad;
    DR_MODE*                     drawMode;
    s32                          depth;
    s32                          segment;
#if WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
    GameLocationKey* location;
#endif

    /// Queues the completed quad and its draw mode into one depth bucket.
    ///
    /// Captures `quad`, `depth`, `drawMode`, the instance's byte cursor and the
    /// current display table. Takes no arguments and is undefined before
    /// leaving this function. Prepending the draw mode last makes the GPU
    /// consume it before the quad. This statement list must be invoked inside
    /// a braced block, never as an unbraced branch.
#define WATER_WAVE_STRIPS_QUEUE_QUAD()                                                                                                                \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad); \
    drawMode                        = (DR_MODE*)WATER_WAVE_STRIPS_PACKET_CURSOR;                                                                      \
    WATER_WAVE_STRIPS_PACKET_CURSOR = (u8*)(drawMode + 1);                                                                                            \
    setDrawTPage(drawMode, 0, 0, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));                                                                            \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), drawMode)

#if WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
    surface   = WATER_WAVE_STRIPS_SURFACES;
    wavePhase = -(gDisplayState.animFrame * WATER_WAVE_STRIPS_PHASE_PER_FRAME);
    location  = &gGameSession->location.loc;
    // These corridor views omit water, before reserving either packet or scratch space.
    if (location->stage == GAME_STAGE_MINE_SHELTER) {
        if (location->area == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
            if ((u32)(gGameSession->location.loc.view - WATER_WAVE_STRIPS_FIRST_HIDDEN_VIEW) < WATER_WAVE_STRIPS_HIDDEN_VIEW_COUNT) {
                return;
            }
        }
    }
    // Borrow the unused actor-load area, split by the display's buffer index.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        WATER_WAVE_STRIPS_PACKET_CURSOR = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * WATER_WAVE_STRIPS_PACKET_HALF_BYTES;
    } else {
        WATER_WAVE_STRIPS_PACKET_CURSOR = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * WATER_WAVE_STRIPS_PACKET_HALF_BYTES;
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
