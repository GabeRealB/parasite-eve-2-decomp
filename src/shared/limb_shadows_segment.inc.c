/* Part of the limb shadows library; see limb_shadows.h. */

/// Draws one textured, semi-transparent quad for the segment between model
/// parts `firstJoint` and `secondJoint`, laid flat at the world-space height
/// `height`. The quad is `width` wide on each side of the segment and
/// stretches half the segment's length past each end; it is tinted grey by
/// `shade` and skipped when the projection clips it. Equal parts draw nothing.
void LIMB_SHADOW_DRAW_SEGMENT(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorLimbShadowScratch* s;
    s16                     angle;
    GfxCoord*               secondCoord;
    GfxCoord*               firstCoord;
    s32                     offset0;
    s32                     offset1;
    s32                     offset2;
    s32                     offset3;
    s32                     halfX;
    s32                     halfZ;
    GfxCoord*               coords;
    GfxCoord*               view;
    POLY_FT4*               poly;

    coords      = actor->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = SCRATCH_STACK_RESERVE_BLOCK(ActorLimbShadowScratch);
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->firstPos.vy   = height;
        s->secondPos.vy  = height;
        s->firstPos.vx   = s->firstMatrix.t[0];
        s->firstPos.vz   = s->firstMatrix.t[2];
        s->secondPos.vx  = s->secondMatrix.t[0];
        s->secondPos.vz  = s->secondMatrix.t[2];
        angle            = ratan2(s->secondPos.vx - s->firstPos.vx, s->secondPos.vz - s->firstPos.vz);
        halfX            = (s->firstPos.vx - s->secondPos.vx) / 2;
        halfZ            = (s->firstPos.vz - s->secondPos.vz) / 2;
        offset0          = rcos(angle) * width;
        s->corners[0].vy = height;
        s->corners[0].vx = halfX + (s->firstPos.vx - (offset0 >> 0xC));
        s->corners[0].vz = halfZ + (s->firstPos.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1          = rcos(angle) * width;
        s->corners[1].vy = height;
        s->corners[1].vx = halfX + (s->firstPos.vx + (offset1 >> 0xC));
        s->corners[1].vz = halfZ + (s->firstPos.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2          = rcos(angle) * width;
        s->corners[2].vy = height;
        s->corners[2].vx = (s->secondPos.vx - (offset2 >> 0xC)) - halfX;
        s->corners[2].vz = (s->secondPos.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3          = rcos(angle) * width;
        s->corners[3].vy = height;
        s->corners[3].vx = (s->secondPos.vx + (offset3 >> 0xC)) - halfX;
        s->corners[3].vz = (s->secondPos.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `gGfxViewCoord.workm`, whose high half the GTE loads below share. */
        view               = &gGfxViewCoord;
        view->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(view);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corners[0], &s->corners[1], &s->corners[2], &s->corners[3], &s->screenCorners[0], &s->screenCorners[1],
                                 &s->screenCorners[2], &s->screenCorners[3], &s->depthCue, &s->flag);
        if (s->flag >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screenCorners[0];
            GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screenCorners[1];
            GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screenCorners[2];
            GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screenCorners[3];
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorLimbShadowScratch);
    }
}
