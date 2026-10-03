/* Part of the glow drawing library; see glow_draw.h. */

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `gSaloonLightPoints`;
/// each root's tip lies at four times its offset to a later entry (15 and 18
/// for the first shaft, 16 and 19 for the second). All four corners are
/// moved to world space through `coord->workm` and projected through
/// `GsWSMATRIX`. The roots take a grey of 0x20 or 0x30 depending on the
/// parity of `gDisplayState.animFrame` and the tips are black, so the shaft
/// fades outward. The roots are the quad's corners 0 and 1 and the tips its
/// corners 2 and 3; the quad is sorted by the last tip's depth and skipped
/// when that is below 0x11.
void glowDrawTwinShafts(GfxCoord* coord)
{
    EffectQuadCornersScratch* block;
    POLY_G4*                  prim;
    SVECTOR*                  dirA;
    SVECTOR*                  dirB;
    s32                       i;
    s32                       j;
    s32                       rgb;

    block = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gSaloonLightPoints[14]);
    gte_rtv0();
    gte_stsv(&block->vertices[0]);
    (u16) block->vertices[0].vx = (u16)block->vertices[0].vx + (u16)coord->workm.t[0];
    (u16) block->vertices[0].vy = (u16)block->vertices[0].vy + (u16)coord->workm.t[1];
    (u16) block->vertices[0].vz = (u16)block->vertices[0].vz + (u16)coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gSaloonLightPoints[17]);
    gte_rtv0();
    gte_stsv(&block->vertices[1]);
    (u16) block->vertices[1].vx = (u16)block->vertices[1].vx + (u16)coord->workm.t[0];
    (u16) block->vertices[1].vy = (u16)block->vertices[1].vy + (u16)coord->workm.t[1];
    (u16) block->vertices[1].vz = (u16)block->vertices[1].vz + (u16)coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                           = i + 15;
        dirA                        = &gSaloonLightPoints[j];
        (u16) block->vertices[2].vx = (u16)gSaloonLightPoints[14].vx +
                                      ((u16)dirA->vx - (u16)gSaloonLightPoints[14].vx) * 4;
        (u16) block->vertices[2].vy = (u16)gSaloonLightPoints[14].vy +
                                      ((u16)dirA->vy - (u16)gSaloonLightPoints[14].vy) * 4;
        (u16) block->vertices[2].vz = (u16)gSaloonLightPoints[14].vz +
                                      ((u16)dirA->vz - (u16)gSaloonLightPoints[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->vertices[2]);
        gte_rtv0();
        gte_stsv(&block->vertices[2]);
        (u16) block->vertices[2].vx = (u16)block->vertices[2].vx + (u16)coord->workm.t[0];
        (u16) block->vertices[2].vy = (u16)block->vertices[2].vy + (u16)coord->workm.t[1];
        (u16) block->vertices[2].vz = (u16)block->vertices[2].vz + (u16)coord->workm.t[2];

        j                           = i + 18;
        dirB                        = &gSaloonLightPoints[j];
        (u16) block->vertices[3].vx = (u16)gSaloonLightPoints[17].vx +
                                      ((u16)dirB->vx - (u16)gSaloonLightPoints[17].vx) * 4;
        (u16) block->vertices[3].vy = (u16)gSaloonLightPoints[17].vy +
                                      ((u16)dirB->vy - (u16)gSaloonLightPoints[17].vy) * 4;
        (u16) block->vertices[3].vz = (u16)gSaloonLightPoints[17].vz +
                                      ((u16)dirB->vz - (u16)gSaloonLightPoints[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->vertices[3]);
        gte_rtv0();
        gte_stsv(&block->vertices[3]);
        (u16) block->vertices[3].vx = (u16)block->vertices[3].vx + (u16)coord->workm.t[0];
        (u16) block->vertices[3].vy = (u16)block->vertices[3].vy + (u16)coord->workm.t[1];
        (u16) block->vertices[3].vz = (u16)block->vertices[3].vz + (u16)coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vertices[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->vertices[1], &block->vertices[2],
                 &block->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->depth);
        if (block->depth >= 0x11) {
            rgb = ((u8)gDisplayState.animFrame & 1) * 16 + 0x20;
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            setRGB0(prim, rgb, rgb, rgb);
            setRGB1(prim, rgb, rgb, rgb);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
