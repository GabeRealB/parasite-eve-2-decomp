/* Part of the water effects library; see water_effects.h. */

#ifndef WATER_WAVE_STRIPS_FUNC
#define WATER_WAVE_STRIPS_FUNC waterDrawWaveStrips
#endif
#ifndef WATER_WAVE_STRIPS_SURFACE_T
#define WATER_WAVE_STRIPS_SURFACE_T RoomWaterSurface
#endif
#ifndef WATER_WAVE_STRIPS_OWN_CURSOR
#define WATER_WAVE_STRIPS_OWN_CURSOR 0
#endif
#ifndef WATER_WAVE_STRIPS_NEAR_COLOURS
/* red outer edge, grey seam */
#define WATER_WAVE_STRIPS_NEAR_COLOURS(p) \
    (p)->r0 = 0x80;                       \
    (p)->r1 = 0x80;                       \
    (p)->g0 = 0;                          \
    (p)->b0 = 0;                          \
    (p)->g1 = 0;                          \
    (p)->b1 = 0;                          \
    (p)->r2 = 0x20;                       \
    (p)->g2 = 0x20;                       \
    (p)->b2 = 0x20;                       \
    (p)->r3 = 0x20;                       \
    (p)->g3 = 0x20;                       \
    (p)->b3 = 0x20
#endif
#ifndef WATER_WAVE_STRIPS_FAR_COLOURS
#define WATER_WAVE_STRIPS_FAR_COLOURS(p) \
    (p)->r2 = 0x80;                      \
    (p)->r3 = 0x80;                      \
    (p)->g2 = 0;                         \
    (p)->b2 = 0;                         \
    (p)->g3 = 0;                         \
    (p)->b3 = 0;                         \
    (p)->r0 = 0x20;                      \
    (p)->g0 = 0x20;                      \
    (p)->b0 = 0x20;                      \
    (p)->r1 = 0x20;                      \
    (p)->g1 = 0x20;                      \
    (p)->b1 = 0x20
#endif

/// Draws each surface of WATER_WAVE_STRIPS_SURFACES at height
/// WATER_WAVE_STRIPS_HEIGHT as two strips of 16 semi-transparent gouraud quads
/// side by side along X, each running along Z and projected through the view
/// matrix. The seam between them is lifted by a sine wave along Z, scrolling
/// with the display frame (amplitude `rsin >> WATER_WAVE_STRIPS_WAVE_SHIFT`);
/// each quad is followed by a draw-mode packet selecting blend mode 2. The
/// quads come from the room's own cursor WATER_WAVE_STRIPS_PRIM_CURSOR, which
/// WATER_WAVE_STRIPS_OWN_CURSOR 1 resets each frame (the B2 main corridor).
/// The strips' vertex colours are WATER_WAVE_STRIPS_NEAR_COLOURS(poly) and
/// WATER_WAVE_STRIPS_FAR_COLOURS(poly).
static void WATER_WAVE_STRIPS_FUNC(Task* task)
{
    SVECTOR                      v0, v1, v2, v3;
    long                         sxy0, sxy1, sxy2, sxy3;
    long                         p, flag;
    s32                          phase;
    WATER_WAVE_STRIPS_SURFACE_T* e;
    RoomWaterScratch*            w;
    u8*                          head;
    POLY_G4*                     poly;
    DR_MODE*                     dr;
    s32                          otz;
    s32                          i;
#if WATER_WAVE_STRIPS_OWN_CURSOR
    GameLocationKey* k;
#endif

#if WATER_WAVE_STRIPS_OWN_CURSOR
    e     = WATER_WAVE_STRIPS_SURFACES;
    phase = -(gDisplayState.animFrame * 16);
    k     = &gGameSession->location.loc;
    /* not drawn in views 10 and 11 of stage 4, area 0x21 */
    if (k->stage == 4) {
        if (k->area == 0x21) {
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
    head                       = SCRATCH_STACK_CURSOR(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
#else
    e                          = WATER_WAVE_STRIPS_SURFACES;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    head                       = SCRATCH_STACK_CURSOR(u8);
    phase                      = -(gDisplayState.animFrame * 16);
#endif
    SCRATCH_STACK_CURSOR(u8) = head - 0xC;
    w                        = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = WATER_WAVE_STRIPS_HEIGHT;
    for (; e->count != -1; e++) {
        w->dx = e->width / 2;
        w->dz = e->depth / 16;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x;
            v0.vy   = w->y;
            v0.vz   = w->z + w->dz * i;
            v1.vx   = w->x;
            v1.vy   = w->y;
            v1.vz   = w->z + w->dz * (i + 1);
            w->wave = (u32)rsin(phase + (i << 9)) >> WATER_WAVE_STRIPS_WAVE_SHIFT;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> WATER_WAVE_STRIPS_WAVE_SHIFT;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                          = (POLY_G4*)WATER_WAVE_STRIPS_PRIM_CURSOR;
                WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                WATER_WAVE_STRIPS_NEAR_COLOURS(poly);
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
            w->wave = (u32)rsin(phase + (i << 9)) >> WATER_WAVE_STRIPS_WAVE_SHIFT;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> WATER_WAVE_STRIPS_WAVE_SHIFT;
            v1.vx   = w->x + w->dx;
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz * (i + 1);
            v2.vx   = w->x + w->dx * 2;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * i;
            v3.vx   = w->x + w->dx * 2;
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                          = (POLY_G4*)WATER_WAVE_STRIPS_PRIM_CURSOR;
                WATER_WAVE_STRIPS_PRIM_CURSOR = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                WATER_WAVE_STRIPS_FAR_COLOURS(poly);
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
    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

#undef WATER_WAVE_STRIPS_FUNC
#undef WATER_WAVE_STRIPS_SURFACES
#undef WATER_WAVE_STRIPS_HEIGHT
#undef WATER_WAVE_STRIPS_NEAR_COLOURS
#undef WATER_WAVE_STRIPS_FAR_COLOURS
#undef WATER_WAVE_STRIPS_SURFACE_T
#undef WATER_WAVE_STRIPS_OWN_CURSOR
#undef WATER_WAVE_STRIPS_WAVE_SHIFT
#undef WATER_WAVE_STRIPS_PRIM_CURSOR
