/* Part of the falling leaves library; see falling_leaves.h. */

/// Draws one mote: the unit quad `D_80111E38` scaled by `arg1`, rotated and
/// placed by the mote's coordinate frame, then projected through
/// `GsWSMATRIX` into a textured quad. A mote nearer than `otz` 0x11 is not
/// drawn. `arg2` is the fade level: zero draws the texture unshaded, anything
/// else modulates it to that grey and draws it semi-transparent.
void leafDraw(GfxCoord* coord, s32 arg1, s16 arg2)
{
    EffectQuadCornersScratch* blk;
    POLY_FT4*                 prim;
    SVECTOR*                  sv;
    s32                       i;

    blk = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        blk->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * arg1;
        // Spelled as an offset rather than `&blk->vertices[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(EffectQuadCornersScratch, vertices));
        sv->vy = 0;
        sv->vz = (u16)D_80111E38[i].axis1Sign * arg1;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vertices[i]);
        gte_rtv0();
        gte_stsv(&blk->vertices[i]);
        blk->vertices[i].vx += coord->workm.t[0];
        sv->vy              += coord->workm.t[1];
        sv->vz              += coord->workm.t[2];
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->vertices[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->vertices[1], &blk->vertices[2], &blk->vertices[3]);
    gte_rtpt();
    setUV4(prim, 0, 0xE8, 7, 0xE8, 0, 0xEF, 7, 0xEF);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->depth);
    if (blk->depth >= 0x11) {
        if (arg2 != 0) {
            setRGB0(prim, arg2, arg2, arg2);
            setSemiTrans(prim, 1);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
