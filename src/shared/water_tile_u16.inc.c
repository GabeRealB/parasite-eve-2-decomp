/* Part of the water effects library; see water_effects.h. */

/// Draws an upright sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued, its texture the 56-texel
/// cell `arg1 & 7` of a four-wide, two-row grid starting at v 0x70. The quad
/// is `2 * r` on a side with `r = (s16)arg2 * 55 / otz`, and the projected
/// point sits a quarter of the way up from its bottom edge. The work block
/// lives on the scratchpad stack.
void waterDrawTileU16(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    u16            idx;
    u32            cell;
    s32            row;
    u8             u0;
    u8             u1;
    u8             v0;
    u8             v1;

    idx           = arg1;
    block         = SCRATCH_STACK_RESERVE_BLOCK(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = idx;
        u0          = (cell & 3) * 0x38;
        row         = ((cell & 7) >> 2) * 0x38;
        v0          = row + 0x70;
        v1          = row + 0xA7;
        u1          = u0 + 0x37;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        block->step = ((s16)arg2 * 55) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}
