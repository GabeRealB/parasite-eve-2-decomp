/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a beam of light from 24 screen points laid out as four rings of six
/// around the two ends of a segment (`points`, sorted at `otz` plus
/// GLOW_DRAW_RING_BEAM_OT_OFFSET): six additive gouraud quads for the bright
/// core, then ten for the halo out to the outer rings, which fade to black.
/// `gGlowRingBeamQuads` names each quad's corners and `gGlowRingBeamColors` each
/// point's colour, scaled by GLOW_DRAW_RING_BEAM_BRIGHTNESS(task); the halo
/// is blended by the DR_TPAGE word GLOW_DRAW_RING_BEAM_HALO_TPAGE. A unit that
/// defines GLOW_DRAW_RING_BEAM_PHASE_STEP advances the task's `killCountdown`
/// pulse phase by it here.
void glowDrawRingBeam(Task* task, SVECTOR* points, s32 otz)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad  = gGlowRingBeamQuads[0];
    scale = GLOW_DRAW_RING_BEAM_BRIGHTNESS(task);
#ifdef GLOW_DRAW_RING_BEAM_PHASE_STEP
    task->killCountdown += GLOW_DRAW_RING_BEAM_PHASE_STEP;
#endif
    if (task->killCountdown >= 0x800) {
        task->killCountdown = 0;
    }
    rsin(task->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (gGlowRingBeamColors[i][0] * (s16)scale) >> 12;
        colors[i].g = (gGlowRingBeamColors[i][1] * (s16)scale) >> 12;
        colors[i].b = (gGlowRingBeamColors[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = points[a].vx;
        poly->y0   = points[a].vy;
        poly->x1   = points[b].vx;
        poly->y1   = points[b].vy;
        poly->x2   = points[c].vx;
        poly->y2   = points[c].vy;
        poly->x3   = points[d].vx;
        poly->y3   = points[d].vy;
        addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + GLOW_DRAW_RING_BEAM_OT_OFFSET, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + GLOW_DRAW_RING_BEAM_OT_OFFSET, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = points[a].vx;
        poly->y0   = points[a].vy;
        poly->x1   = points[b].vx;
        poly->y1   = points[b].vy;
        poly->x2   = points[c].vx;
        poly->y2   = points[c].vy;
        poly->x3   = points[d].vx;
        poly->y3   = points[d].vy;
        addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + GLOW_DRAW_RING_BEAM_OT_OFFSET, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = GLOW_DRAW_RING_BEAM_HALO_TPAGE;
        addPrim((&gGpuCurrentOt[((((u32)(otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + GLOW_DRAW_RING_BEAM_OT_OFFSET, tpage);
        quad += 4;
    }
}

#undef GLOW_DRAW_RING_BEAM_BRIGHTNESS
#undef GLOW_DRAW_RING_BEAM_PHASE_STEP
#undef GLOW_DRAW_RING_BEAM_OT_OFFSET
#undef GLOW_DRAW_RING_BEAM_HALO_TPAGE
