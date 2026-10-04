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

#ifndef WATER_WAVE_STRIPS_FUNC
#define WATER_WAVE_STRIPS_FUNC waterDrawWaveStrips
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
/// `WATER_WAVE_STRIPS_PRIM_CURSOR` must bind a writable `u8*` lvalue. The caller
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

/// Draws each surface of WATER_WAVE_STRIPS_SURFACES at height
/// WATER_WAVE_STRIPS_HEIGHT as two strips of 16 semi-transparent gouraud quads
/// side by side along X, each running along Z and projected through the view
/// matrix. The seam between them is lifted by a sine wave along Z, scrolling
/// with the display frame (scaled by `WATER_WAVE_STRIPS_AMPLITUDE_SHIFT`);
/// each quad is followed by a draw-mode packet selecting blend mode 2. The
/// packets append to `WATER_WAVE_STRIPS_PRIM_CURSOR`, with initialization
/// selected by `WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR`.
/// The strips' vertex colours are WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(poly) and
/// WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(poly).
static void WATER_WAVE_STRIPS_FUNC(Task* task)
{
    SVECTOR                      v0, v1, v2, v3;
    long                         sxy0, sxy1, sxy2, sxy3;
    long                         p, flag;
    s32                          phase;
    WATER_WAVE_STRIPS_SURFACE_T* surface;
    WaterQuadScratch*            scratch;
    WaterQuadScratch*            scratchEnd;
    POLY_G4*                     poly;
    DR_MODE*                     dr;
    s32                          otz;
    s32                          i;
#if WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
    GameLocationKey* k;
#endif

#if WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
    surface = WATER_WAVE_STRIPS_SURFACES;
    phase   = -(gDisplayState.animFrame * 16);
    k       = &gGameSession->location.loc;
    /* not drawn in views 10 and 11 of stage 4, area 0x21 */
    if (k->stage == GAME_STAGE_MINE_SHELTER) {
        if (k->area == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
            if ((u32)(gGameSession->location.loc.view - 0xA) < 2) {
                return;
            }
        }
    }
    /* the strips are written to this buffer's half of the actor load area */
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
#else
    surface                    = WATER_WAVE_STRIPS_SURFACES;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    phase                      = -(gDisplayState.animFrame * 16);
#endif
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->y = WATER_WAVE_STRIPS_HEIGHT;
    for (; !WATER_WAVE_STRIPS_IS_LIST_END(surface); surface++) {
        scratch->dx = surface->width / 2;
        scratch->dz = surface->depth / 16;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < 16; i++) {
            v0.vx            = scratch->x;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z + scratch->dz * i;
            v1.vx            = scratch->x;
            v1.vy            = scratch->y;
            v1.vz            = scratch->z + scratch->dz * (i + 1);
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            v2.vx            = scratch->x + scratch->dx;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz * i;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            v3.vx            = scratch->x + scratch->dx;
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz * (i + 1);
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                          = (POLY_G4*)WATER_WAVE_STRIPS_PRIM_CURSOR;
                WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(poly);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                            = (DR_MODE*)WATER_WAVE_STRIPS_PRIM_CURSOR;
                WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            scratch->yOffset = (u32)rsin(phase + (i << 9)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            v0.vx            = scratch->x + scratch->dx;
            v0.vy            = scratch->y + scratch->yOffset;
            v0.vz            = scratch->z + scratch->dz * i;
            scratch->yOffset = (u32)rsin(phase + ((i + 1) << 9)) >> WATER_WAVE_STRIPS_AMPLITUDE_SHIFT;
            v1.vx            = scratch->x + scratch->dx;
            v1.vy            = scratch->y + scratch->yOffset;
            v1.vz            = scratch->z + scratch->dz * (i + 1);
            v2.vx            = scratch->x + scratch->dx * 2;
            v2.vy            = scratch->y;
            v2.vz            = scratch->z + scratch->dz * i;
            v3.vx            = scratch->x + scratch->dx * 2;
            v3.vy            = scratch->y;
            v3.vz            = scratch->z + scratch->dz * (i + 1);
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                          = (POLY_G4*)WATER_WAVE_STRIPS_PRIM_CURSOR;
                WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(poly);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                            = (DR_MODE*)WATER_WAVE_STRIPS_PRIM_CURSOR;
                WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

#undef WATER_WAVE_STRIPS_FUNC
#undef WATER_WAVE_STRIPS_SURFACES
#undef WATER_WAVE_STRIPS_HEIGHT
#undef WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS
#undef WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS
#undef WATER_WAVE_STRIPS_SURFACE_T
#undef WATER_WAVE_STRIPS_IS_LIST_END
#undef WATER_WAVE_STRIPS_RESET_ACTOR_LOAD_CURSOR
#undef WATER_WAVE_STRIPS_AMPLITUDE_SHIFT
#undef WATER_WAVE_STRIPS_PRIM_CURSOR
