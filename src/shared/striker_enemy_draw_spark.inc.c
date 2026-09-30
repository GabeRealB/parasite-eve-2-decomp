/* Part of the striker enemy library; see striker_enemy.h. */

/// Links one frame of the rotating impact-spark billboard at `arg0`'s world
/// position, projected through `GsWSMATRIX` by a single `RTPS`; a negative
/// projection flag drops the quad.  `arg1` picks one of six 0x27-square frames
/// along row 0x38 of tpage 0x2A, `arg2` sizes the quad and `arg3` spins it: the
/// corners sit `arg2 * 0x27 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the spark shrinks with depth.
void strikerDrawSpark(GfxCoord* arg0, u16 arg1, u16 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    GpEffFlareScratch* blk;
    GpEffFlareScratch* copy;
    POLY_FT4*          prim;
    s32                ang;
    u16                frame;
    s32                u;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    blk                            = (GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch));
    copy                           = blk;
    blk->vec.vx                    = (u16)arg0->workm.t[0];
    blk->vec.vy                    = (u16)arg0->workm.t[1];
    blk->vec.vz                    = (u16)arg0->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->vec);
    gte_rtps();
    gte_stsxy(&((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->sx);
    gte_stflg(&((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(copy);
        ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x2A;
        prim->clut  = 0x4293;
        frame       = arg1 % 6;
        u           = frame * 0x28;
        setUV4(prim, u, 0x38, u + 0x27, 0x38, u, 0x5F, u + 0x27, 0x5F);
        ang      = (s16)arg3;
        blk->dx  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rcos(ang)) >> 12;
        prim->x0 = blk->sx + (u16)blk->dx;
        prim->x3 = blk->sx - (u16)blk->dx;
        prim->y0 = blk->sy - (u16)blk->dy;
        prim->y3 = blk->sy + (u16)blk->dy;
        ang      = ang + 0x400;
        blk->dx  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rcos(ang)) >> 12;
        prim->x1 = blk->sx + (u16)blk->dx;
        prim->x2 = blk->sx - (u16)blk->dx;
        prim->y1 = blk->sy - (u16)blk->dy;
        prim->y2 = blk->sy + (u16)blk->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(GpEffFlareScratch));
}
