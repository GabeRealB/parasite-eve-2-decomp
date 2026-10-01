/* Part of the glow drawing library; see glow_draw.h. */

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues four gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / otz`. `arg2` packs the centre vertex's colour, four
/// bits per channel (R, G, B from high to low nibble), with the frame
/// counter's low bit as a flicker; the rim is black. As for glowDrawCapsule,
/// GLOW_DRAW_DISC_PULL draws a centre beyond depth 0x50 that much nearer and
/// GLOW_DRAW_DISC_SHIFTED_FLICKER adds the frame bit shifted by the colour's
/// top nibble.
void glowDrawDisc(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    GLOW_DRAW_DISC_SCRATCH* block;
    POLY_G4*                prim;
    s32                     ang;
    s32                     t;
    s32                     t2;
    s32                     packed;
    s32                     blend;
    s32                     tr;
    s32                     tg;
    u8                      r;
    u8                      g;
    u8                      b;
#ifdef GLOW_DRAW_DISC_PULL
    s32 otz;
#endif

    block = SCRATCH_STACK_RESERVE_BLOCK(GLOW_DRAW_DISC_SCRATCH);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
#ifdef GLOW_DRAW_DISC_PULL
        otz = block->otz;
        if (otz > 0x50) {
            block->otz = otz - GLOW_DRAW_DISC_PULL;
        }
        /* this build scales the half-extent with a pair of shifts */
        arg1 = arg1 << 16;
        arg1 = arg1 >> 10;
        arg1 = arg1 / block->otz;
#else
        arg1 = ((s16)arg1 * 64) / block->otz;
#endif
        ang = 0;
#if GLOW_DRAW_DISC_SHIFTED_FLICKER
        /* the flicker bit, shifted by the colour word's top nibble, is added
           to each channel */
        packed = arg2 << 16;
        blend  = (gDisplayState.animFrame & 1) << (packed >> 28);
#else
        blend  = ((u8)gDisplayState.animFrame & 1) * 8;
        packed = arg2 << 16;
#endif
        tr = (packed >> 20) & 0xF0;
        tg = (packed >> 16) & 0xF0;
#if GLOW_DRAW_DISC_SHIFTED_FLICKER
        r = blend + tr;
        g = blend + tg;
        b = blend + ((arg2 & 0xF) << 4);
#else
        r = blend | tr;
        g = blend | tg;
        b = blend | ((arg2 & 0xF) << 4);
#endif
        block->radius = arg1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GLOW_DRAW_DISC_SCRATCH);
}

#undef GLOW_DRAW_DISC_PULL
#undef GLOW_DRAW_DISC_SHIFTED_FLICKER
