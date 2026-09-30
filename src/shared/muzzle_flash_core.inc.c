/* Part of the muzzle flash library; see muzzle_flash.h. */

void muzzleFlashDrawCore(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                   head;
    OverlaySpriteScratch* blk;
    OverlaySpriteScratch* otzp;
    POLY_FT4*             prim;
    s32                   ang;
    u16                   vz;

    head                                       = SCRATCH_STACK_CURSOR(u8);
    blk                                        = (OverlaySpriteScratch*)(head - sizeof(OverlaySpriteScratch));
    blk->vec.vx                                = (u16)arg0->workm.t[0];
    blk->vec.vy                                = (u16)arg0->workm.t[1];
    vz                                         = (u16)arg0->workm.t[2];
    otzp                                       = blk;
    SCRATCH_STACK_CURSOR(OverlaySpriteScratch) = blk;
    blk->vec.vz                                = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((OverlaySpriteScratch*)(head - 0x18))->vec);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((OverlaySpriteScratch*)(head - 0x18))->sxy);
    gte_stszotz(&otzp->otz);
    if (((OverlaySpriteScratch*)(head - 0x18))->otz >= 0x11) {
        ang         = arg2;
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        setcode(prim, getcode(prim) | 3);
        blk->dx  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rcos(ang)) >> 12;
        prim->x0 = (u16)blk->sxy.vx + (u16)blk->dx;
        prim->x3 = (u16)blk->sxy.vx - (u16)blk->dx;
        prim->y0 = (u16)blk->sxy.vy - (u16)blk->dy;
        ang      = ang + 0x400;
        prim->y3 = (u16)blk->sxy.vy + (u16)blk->dy;
        blk->dx  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rcos(ang)) >> 12;
        prim->x1 = (u16)blk->sxy.vx + (u16)blk->dx;
        prim->x2 = (u16)blk->sxy.vx - (u16)blk->dx;
        prim->y1 = (u16)blk->sxy.vy - (u16)blk->dy;
        prim->y2 = (u16)blk->sxy.vy + (u16)blk->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((OverlaySpriteScratch*)(head - 0x18))->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlaySpriteScratch));
}
