/* Part of the Dryfield main street library; see main_street.h. */

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative and `depth` is at least 0x41, queues one
/// semi-transparent shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4383) rotated
/// about the projected centre. `arg1` selects a 48-texel UV tile in a 5-wide
/// grid: u = `(arg1 % 5) * 48`, v = `(arg1 / 5) * 48 - 0x80`. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 47 / depth`.
/// `arg3` is the spin angle, applied at `arg3` and `arg3 + 0x400` through
/// `rsin`/`rcos`.
void mainStreetDrawPuff(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**                  scratch;
    EffectBillboardScratch* scratchHead;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               prim;
    s32                     ang;
    s32                     ang2;
    s32                     sine;
    s32                     span;
    s32                     u0;
    s32                     v0;
    s32                     u1;
    s32                     v1;
    u16                     vz;
    u16                     tex;

    scratch              = SCRATCH_HEAD_ADDR;
    scratchHead          = SCRATCH_HEAD_AT(scratch, EffectBillboardScratch);
    block                = scratchHead - 1;
    block->worldPoint.vx = (u16)arg0->workm.t[0];
    block->worldPoint.vy = (u16)arg0->workm.t[1];
    vz                   = (u16)arg0->workm.t[2];
    depthOutput          = &block->depth;
    *scratch             = block;
    block->worldPoint.vz = vz;
    tex                  = arg1;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        if (block->depth >= 0x41) {
            ang         = (s16)arg3;
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 3;
            u0          = (tex % 5) * 0x30;
            v0          = (tex / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 - 0x51;
            v0          = v0 - 0x80;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            sine                 = rsin(ang);
            span                 = (s16)arg2 * 0x2F;
            block->cornerOffsetX = ((span / block->depth) * sine) >> 12;
            block->cornerOffsetY = ((span / block->depth) * rcos(ang)) >> 12;
            prim->x0             = block->screenX + (u16)block->cornerOffsetX;
            prim->x3             = block->screenX - (u16)block->cornerOffsetX;
            prim->y0             = block->screenY - (u16)block->cornerOffsetY;
            prim->y3             = block->screenY + (u16)block->cornerOffsetY;
            ang2                 = ang + 0x400;
            block->cornerOffsetX = ((span / block->depth) * rsin(ang2)) >> 12;
            block->cornerOffsetY = ((span / block->depth) * rcos(ang2)) >> 12;
            prim->x1             = block->screenX + (u16)block->cornerOffsetX;
            prim->x2             = block->screenX - (u16)block->cornerOffsetX;
            prim->y1             = block->screenY - (u16)block->cornerOffsetY;
            prim->y2             = block->screenY + (u16)block->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}
