/* Part of the water hole library; see water_hole.h. */

/// Draws each surface in `gWaterHoleSurfaces` as two strips of 64
/// semi-transparent Gouraud quads laid side by side along Z, projected through
/// the view matrix. The seam between the strips is lifted by a sine wave that
/// runs along X and scrolls with `gWaterHoleWaveScroll`, which only
/// advances while `gSceneCombatState.actorControl` is clear. The outer edges are coloured
/// (0xFF, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. `task` is unused.
void waterHoleDrawSurfaces(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    s32               step;
    s32               phase;
    WaterHoleSurface* surface;
    POLY_G4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;
    s32               half;
    s32               wave;

    surface = gWaterHoleSurfaces;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gWaterHolePrimCursor = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        gWaterHolePrimCursor = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        gWaterHoleWaveScroll++;
    }
    phase                      = -(gWaterHoleWaveScroll * 16);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (; surface->y != WATER_SURFACE_LIST_END; surface++) {
        step = surface->width / 64;
        half = surface->depth / 2;
        for (i = 0; i < 64; i++) {
            v0.vx = surface->x + step * i;
            v0.vy = surface->y;
            v0.vz = surface->z;
            v1.vx = surface->x + step * (i + 1);
            v1.vy = surface->y;
            v1.vz = surface->z;
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v2.vx = surface->x + step * i;
            v2.vy = surface->y + wave;
            v2.vz = surface->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v3.vx = surface->x + step * (i + 1);
            v3.vy = surface->y + wave;
            v3.vz = surface->z + half;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                 = (POLY_G4*)gWaterHolePrimCursor;
                gWaterHolePrimCursor = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0xFF;
                poly->r1                       = 0xFF;
                poly->g0                       = 0;
                poly->b0                       = 0;
                poly->g1                       = 0;
                poly->b1                       = 0;
                poly->r2                       = 0x20;
                poly->g2                       = 0x20;
                poly->b2                       = 0x20;
                poly->r3                       = 0x20;
                poly->g3                       = 0x20;
                poly->b3                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                   = (DR_MODE*)gWaterHolePrimCursor;
                gWaterHolePrimCursor = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 64; i++) {
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v0.vx = surface->x + step * i;
            v0.vy = surface->y + wave;
            v0.vz = surface->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v1.vx = surface->x + step * (i + 1);
            v1.vy = surface->y + wave;
            v1.vz = surface->z + half;
            v2.vx = surface->x + step * i;
            v2.vy = surface->y;
            v2.vz = surface->z + half * 2;
            v3.vx = surface->x + step * (i + 1);
            v3.vy = surface->y;
            v3.vz = surface->z + half * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                 = (POLY_G4*)gWaterHolePrimCursor;
                gWaterHolePrimCursor = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r2                       = 0xFF;
                poly->r3                       = 0xFF;
                poly->g2                       = 0;
                poly->b2                       = 0;
                poly->g3                       = 0;
                poly->b3                       = 0;
                poly->r0                       = 0x20;
                poly->g0                       = 0x20;
                poly->b0                       = 0x20;
                poly->r1                       = 0x20;
                poly->g1                       = 0x20;
                poly->b1                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                   = (DR_MODE*)gWaterHolePrimCursor;
                gWaterHolePrimCursor = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
}
