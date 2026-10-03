/* Part of the muzzle flash library; see muzzle_flash.h. */

void muzzleFlashDrawCore(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    OverlaySpriteScratch* head;
    OverlaySpriteScratch* blk;
    OverlaySpriteScratch* otzp;
    POLY_FT4*             prim;
    s32                   ang;
    u16                   vz;

    head                                       = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
    blk                                        = head - 1;
    blk->worldPos.vx                           = (u16)arg0->workm.t[0];
    blk->worldPos.vy                           = (u16)arg0->workm.t[1];
    vz                                         = (u16)arg0->workm.t[2];
    otzp                                       = blk;
    SCRATCH_STACK_CURSOR(OverlaySpriteScratch) = blk;
    blk->worldPos.vz                           = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&(head - 1)->worldPos);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&(head - 1)->screenPos);
    gte_stszotz(&otzp->otz);
    if ((head - 1)->otz >= 0x11) {
        ang         = arg2;
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        setcode(prim, getcode(prim) | 3);
        blk->cornerDx = (((arg1 * 55) / (head - 1)->otz) * rsin(ang)) >> 12;
        blk->cornerDy = (((arg1 * 55) / (head - 1)->otz) * rcos(ang)) >> 12;
        prim->x0      = blk->screenPos.vx + blk->cornerDx;
        prim->x3      = blk->screenPos.vx - blk->cornerDx;
        prim->y0      = blk->screenPos.vy - blk->cornerDy;
        ang           = ang + 0x400;
        prim->y3      = blk->screenPos.vy + blk->cornerDy;
        blk->cornerDx = (((arg1 * 55) / (head - 1)->otz) * rsin(ang)) >> 12;
        blk->cornerDy = (((arg1 * 55) / (head - 1)->otz) * rcos(ang)) >> 12;
        prim->x1      = blk->screenPos.vx + blk->cornerDx;
        prim->x2      = blk->screenPos.vx - blk->cornerDx;
        prim->y1      = blk->screenPos.vy - blk->cornerDy;
        prim->y2      = blk->screenPos.vy + blk->cornerDy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(head - 1)->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
}
