/* Part of the glow drawing library; see glow_draw.h. */

/// Draws one glow sprite at the point `arg1`, given in the local space of
/// `arg0`: the point is rotated by the coordinate's `workm` and offset by its
/// translation, then projected through `GsWSMATRIX`. Anything nearer than OTZ
/// 0x11 is dropped. Otherwise one semi-transparent `POLY_FT4` on tpage 0x2B is
/// queued at its OTZ; `(s16)arg2` picks the 40-texel wide texture column and
/// the clut `(arg2 & 0x3F) | 0x4380`, `(s16)arg3` is the half-extent scaled by
/// 39 / OTZ, and the grey level flickers between 0x20 and 0x30 with bit 0 of
/// the display's animation frame. Works in a `RoomGlowSpriteScratch` block.
void glowDrawFlareLocal(GfxCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    RoomGlowSpriteScratch* block;
    POLY_FT4*              prim;
    DisplayState*          ds;
    s32                    su;
    s32                    sv;
    s32                    u0;
    s32                    u1;
    s32                    flip;
    s32                    rgb;
    s16                    xy;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += arg0->workm.t[0];
    block->worldPos.vy += arg0->workm.t[1];
    block->worldPos.vz += arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        ds          = &gDisplayState;
        flip        = (u8)ds->animFrame;
        su          = (s16)arg2;
        sv          = (s16)arg3;
        prim->tpage = 0x2B;
        prim->clut  = (su & 0x3F) | 0x4380;
        u0          = su * 0x28;
        u1          = u0 + 0x27;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->v0    = 0;
        prim->v1    = 0;
        prim->v2    = 0x27;
        prim->v3    = 0x27;
        rgb         = (flip & 1) << 4;
        rgb        += 0x20;
        setSemiTrans(prim, 1);
        prim->r0          = rgb;
        prim->g0          = rgb;
        prim->b0          = rgb;
        block->halfExtent = (sv * 0x27) / block->otz;
        xy                = block->screenPos.vx - block->halfExtent;
        prim->x2          = xy;
        prim->x0          = xy;
        xy                = block->screenPos.vx + block->halfExtent;
        prim->x3          = xy;
        prim->x1          = xy;
        xy                = block->screenPos.vy - block->halfExtent;
        prim->y1          = xy;
        prim->y0          = xy;
        xy                = block->screenPos.vy + block->halfExtent;
        prim->y3          = xy;
        prim->y2          = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}
