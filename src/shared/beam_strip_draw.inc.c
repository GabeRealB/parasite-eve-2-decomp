/* Part of the beam strip library; see beam_strip.h. */

#ifndef BEAM_STRIP_OTZ_BIAS
#define BEAM_STRIP_OTZ_BIAS 1
#endif

static void beamStripDraw(GfxCoord* coord, SVECTOR* end, s32 cell, s16 width)
{
    u8*               head;
    BeamStripScratch* block;
    BeamStripScratch* vecp;
    POLY_FT4*         prim;
    s16               ang;
    u16               vz;

    head                                       = SCRATCH_STACK_CURSOR(u8);
    ((BeamStripScratch*)(head - 0x20))->vec.vx = (u16)coord->workm.t[0];
    block                                      = (BeamStripScratch*)(head - 0x20);
    block->vec.vy                              = (u16)coord->workm.t[1];
    vz                                         = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(BeamStripScratch)     = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((BeamStripScratch*)(head - 0x20))->sxy0);
    gte_stflg(&((BeamStripScratch*)(head - 0x20))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((BeamStripScratch*)(head - 0x20))->otz);
#if BEAM_STRIP_OTZ_BIAS
        block->otz++;
#endif
        gte_ldv0(end);
        gte_rtps();
        gte_stsxy(&((BeamStripScratch*)(head - 0x20))->sxy1);
        gte_stflg(&((BeamStripScratch*)(head - 0x20))->flag);
        if (block->flag >= 0) {
#if BEAM_STRIP_OTZ_BIAS
            block->otz++;
#endif
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4287;
            prim->u0    = (cell & 1) << 7;
            prim->v0    = ((u32)(cell & 3) >> 1) * 24 - 0x30;
            prim->u1    = ((cell & 1) << 7) + 0x7F;
            prim->v1    = ((u32)(cell & 3) >> 1) * 24 - 0x30;
            prim->u2    = (cell & 1) << 7;
            prim->v2    = ((u32)(cell & 3) >> 1) * 24 - 0x19;
            prim->u3    = ((cell & 1) << 7) + 0x7F;
            prim->v3    = ((u32)(cell & 3) >> 1) * 24 - 0x19;
            ang         = ratan2(block->sxy1.vy - block->sxy0.vy, block->sxy1.vx - block->sxy0.vx);
            block->dx   = (((width * 23) / block->otz) * rsin(ang)) >> 12;
            block->dy   = (((width * 23) / block->otz) * rcos(ang)) >> 12;
            prim->x0    = (u16)block->sxy0.vx + (u16)block->dx;
            prim->x3    = (u16)block->sxy1.vx - (u16)block->dx;
            prim->y0    = (u16)block->sxy0.vy - (u16)block->dy;
            prim->y3    = (u16)block->sxy1.vy + (u16)block->dy;
            block->dx   = (((width * 23) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy   = (((width * 23) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1    = (u16)block->sxy1.vx + (u16)block->dx;
            prim->x2    = (u16)block->sxy0.vx - (u16)block->dx;
            prim->y1    = (u16)block->sxy1.vy - (u16)block->dy;
            prim->y2    = (u16)block->sxy0.vy + (u16)block->dy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

#undef BEAM_STRIP_OTZ_BIAS
