/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a flickering tapered beam between `arg1` and `arg2` in `arg0`'s
/// local space. Both points are rotated by the coordinate's `workm`, offset by
/// its translation and projected through `GsWSMATRIX`, using a 0x28-byte block
/// taken from the scratch stack. Nothing is drawn when the far end's `otz` is
/// below 0x11; the near end's is raised to at least 0x10. The ends get the
/// screen radii `(s16)arg3 * 64 / otz`.
///
/// Two passes, a quarter turn apart, each queue three `POLY_G4`s: a wedge of
/// the near end's disc, a quad joining the two ends, and a wedge of the far
/// end's disc walked backwards from a full turn, so the near end covers one
/// half turn and the far end the other. Centre vertices take a grey of 0x20
/// or 0x30 on the parity of `gDisplayState.animFrame`, rim vertices are black.
/// Each primitive goes into the OT bucket of its own end's `otz` with a
/// `Gp_AddTpageShift` tpage.
void glowDrawTaperedBeam(GfxCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    (u16) block->vec0.vx = (u16)block->vec0.vx + (u16)arg0->workm.t[0];
    (u16) block->vec0.vy = (u16)block->vec0.vy + (u16)arg0->workm.t[1];
    (u16) block->vec0.vz = (u16)block->vec0.vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    (u16) block->vec1.vx = (u16)block->vec1.vx + (u16)arg0->workm.t[0];
    (u16) block->vec1.vy = (u16)block->vec1.vy + (u16)arg0->workm.t[1];
    (u16) block->vec1.vz = (u16)block->vec1.vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}
