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
    EffectShapeScratch*             head;
    EffectShapeScratch*             block;
    EffectShapeScratch*             projectionScratch;
    POLY_FT4*                       prim;
    const EffectSpriteTextureFrame* textureFrame;
    u16                             idx;
    s32                             a;
    u16                             vz;

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)pos->vx;
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)pos->vy;
    vz                                       = (u16)pos->vz;
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    projectionScratch                        = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    idx = frame % 12;
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        block->depth   = block->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage            = 0x29;
        textureFrame           = &D_80111E48[idx];
        prim->clut             = getClut(textureFrame->clutX, textureFrame->clutY);
        prim->u0               = textureFrame->u;
        prim->v0               = textureFrame->v;
        prim->u1               = textureFrame->u + 0x27;
        prim->v1               = textureFrame->v;
        prim->u2               = textureFrame->u;
        prim->v2               = textureFrame->v + 0x27;
        prim->u3               = textureFrame->u + 0x27;
        prim->v3               = textureFrame->v + 0x27;
        a                      = ang;
        block->extent.corner.x = (((width * 0x27) / block->depth) * rsin(a)) >> 12;
        block->extent.corner.y = (((width * 0x27) / block->depth) * rcos(a)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        a                      = a + 0x400;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = (((width * 0x27) / block->depth) * rsin(a)) >> 12;
        block->extent.corner.y = (((width * 0x27) / block->depth) * rcos(a)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
