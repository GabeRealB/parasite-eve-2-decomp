/* Part of the falling leaves library; see falling_leaves.h. */

/// Draws the drifting flake as one textured quad lying in the coordinate's
/// local XZ plane: the unit quad `D_80111E38`, scaled by `arg1`, is turned by
/// the coordinate's world matrix, moved to its translation and projected
/// through `GsWSMATRIX`. Unless the GTE flags the projection a `POLY_FT4`
/// (tpage 0x2B, clut 0x4390, an 8x8 texel cell at (0, 0x28)) is queued, raw
/// textured when `arg2` is zero and otherwise semi-transparent at grey level
/// `arg2`.
void leafDraw(GfxCoord* arg0, s32 arg1, s16 arg2)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    POLY_FT4*          prim;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        quadScratch->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * arg1;
        quadScratch->vertices[i].vy = 0;
        quadScratch->vertices[i].vz = (u16)D_80111E38[i].axis1Sign * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&quadScratch->vertices[i]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[i]);
        quadScratch->vertices[i].vx += arg0->workm.t[0];
        quadScratch->vertices[i].vy += arg0->workm.t[1];
        quadScratch->vertices[i].vz += arg0->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (arg2 != 0) {
            setSemiTrans(prim, 1);
            setRGB0(prim, arg2, arg2, arg2);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        prim->v0    = 0x28;
        prim->v1    = 0x28;
        prim->u0    = 0;
        prim->u1    = 7;
        prim->u2    = 0;
        prim->v2    = 0x2F;
        prim->u3    = 7;
        prim->v3    = 0x2F;
        prim->x0    = quadScratch->screenCorners[0].vx;
        prim->y0    = quadScratch->screenCorners[0].vy;
        prim->x1    = quadScratch->screenCorners[1].vx;
        prim->y1    = quadScratch->screenCorners[1].vy;
        prim->x2    = quadScratch->screenCorners[2].vx;
        prim->y2    = quadScratch->screenCorners[2].vy;
        prim->x3    = quadScratch->screenCorners[3].vx;
        prim->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}
