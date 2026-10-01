/* Part of the glow drawing library; see glow_draw.h. */

/// Draws one prism from `gGlowPrismCorners[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring `[4..7]`,
/// then a cap over the lit ring. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The lit
/// corners share a grey of 0x18 plus a small pulse; the far corners are black.
void glowDrawGreyPrism(GfxCoord* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    u8               shade;

    SCRATCH_STACK_RESERVE_BLOCK(RoomQuadScratch);
    blk = SCRATCH_STACK_CURSOR(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&gGlowPrismCorners[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
        blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
        blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&gGlowPrismCorners[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
        blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
        blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&gGlowPrismCorners[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
        blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
        blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&gGlowPrismCorners[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
        blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
        blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gGlowPrismCorners[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
    blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
    blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gGlowPrismCorners[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
    blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
    blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gGlowPrismCorners[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
    blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
    blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gGlowPrismCorners[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
    blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
    blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, shade, shade, shade);
    setRGB1(prim, shade, shade, shade);
    setRGB2(prim, shade, shade, shade);
    setRGB3(prim, shade, shade, shade);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->otz);
    SCRATCH_STACK_RELEASE_BLOCK(RoomQuadScratch);
}
