/* Part of the Pyke flame library; see pyke_flame.h. */

/// Draws one frame of the flying dart: a single semi-transparent, textured
/// `POLY_FT4` billboarded on the world point `pos`. `frame` walks the twelve
/// sprite frames of `D_80111E48`, `width` is the dart's flare width (divided
/// down by the projected depth) and `ang` its spin, so the quad is a square
/// rotated by `ang` rather than an axis-aligned sprite. `otz` is biased by one
/// before it is used as the divisor so a point on the near plane cannot divide
/// by zero.
void pykeFlameDrawBlob(VECTOR3* pos, u16 frame, u16 width, s16 ang)
{
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    GpEffUv8*        rec;
    u16              idx;
    s32              a;
    u16              vz;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)pos->vx;
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)pos->vy;
    vz                                        = (u16)pos->vz;
    SCRATCH_STACK_CURSOR(GpFxQuadScratch)     = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    idx = frame % 12;
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        rec         = &D_80111E48[idx];
        prim->clut  = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
        prim->u0    = rec->u;
        prim->v0    = rec->v;
        prim->u1    = rec->u + 0x27;
        prim->v1    = rec->v;
        prim->u2    = rec->u;
        prim->v2    = rec->v + 0x27;
        prim->u3    = rec->u + 0x27;
        prim->v3    = rec->v + 0x27;
        a           = ang;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        a           = a + 0x400;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}
