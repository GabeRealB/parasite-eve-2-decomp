/* Part of the blade trail library; see blade_trail.h. */

/// Draws the beam as seven Gouraud quads, one per trail slot, walking backwards
/// from `slot`. Each quad spans the near and far trail coordinates of two
/// adjacent slots and fades out along the trail: the leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `flags` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade.
void bladeTrailDraw(s16 slot, s16 flags)
{
    BladeTrailScratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(BladeTrailScratch));
    blk = SCRATCH_STACK_CURSOR(BladeTrailScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &gBladeTrailBase[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        blk->v[0].vy = (u16)a->workm.t[1];
        b            = &gBladeTrailTip[i0];
        blk->v[0].vz = (u16)a->workm.t[2];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        a            = &gBladeTrailBase[i1];
        blk->v[1].vz = (u16)b->workm.t[2];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        b            = &gBladeTrailTip[i1];
        blk->v[2].vz = (u16)a->workm.t[2];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(BladeTrailScratch));
}
