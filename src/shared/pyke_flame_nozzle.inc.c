/* Part of the Pyke flame library; see pyke_flame.h. */

/// Draws one frame of the Pyke's beam head at the world point `pos`. The point
/// is projected through `GsWSMATRIX` by a single `RTPS` into a 0x18-byte
/// scratchpad block; the sprite is dropped whole if that `RTPS` sets its
/// `FLAG`. `frame` walks the six 0x20-wide sprite cells of the strip at
/// `(v = 0x98..0xB7)`, and `brightness` scales the on-screen half-extent, which
/// shrinks with distance as `brightness * 31 / otz`.
void pykeFlameDrawNozzle(VECTOR3* pos, u16 frame, s32 brightness)
{
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    s16            x;
    s16            y;
    u16            uv;
    s32            u0;
    s32            u1;
    u16            vz;

    head                                    = SCRATCH_STACK_CURSOR(u8);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)pos->vx;
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)pos->vy;
    vz                                      = (u16)pos->vz;
    SCRATCH_STACK_CURSOR(GpRingScratch)     = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x430D;
        prim->v0    = 0x98;
        prim->v1    = 0x98;
        prim->v2    = 0xB7;
        prim->v3    = 0xB7;
        /* The remainder has to land in a `u16` of its own: writing it back to
           `frame` lets GCC fold the truncation into the shift, and taking the
           `u0` / `u1` pair straight off `frame` costs the `$a0` / `$a1`
           allocation the ROM has. */
        uv          = frame % 6;
        u0          = uv << 5;
        u1          = u0 + 0x1F;
        prim->u0    = u0;
        prim->u1    = u1;
        prim->u2    = u0;
        prim->u3    = u1;
        block->step = ((u16)brightness * 31) / block->otz;
        x           = (u16)block->sx - (u16)block->step;
        prim->x2    = x;
        prim->x0    = x;
        x           = (u16)block->sx + (u16)block->step;
        prim->x3    = x;
        prim->x1    = x;
        y           = (u16)block->sy - (u16)block->step;
        prim->y1    = y;
        prim->y0    = y;
        y           = (u16)block->sy + (u16)block->step;
        prim->y3    = y;
        prim->y2    = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
