/* Part of the limb shadows library; see limb_shadows.h. */

/// Draws one textured, semi-transparent quad for the segment between model
/// parts `firstJoint` and `secondJoint`, laid flat at the view-space height
/// `height`. The quad is `width` wide on each side of the segment and
/// stretches half the segment's length past each end; it is tinted grey by
/// `shade` and skipped when the projection clips it. Equal parts draw nothing.
void limbShadowDrawSegment(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorBeamScratch* s;
    s16               angle;
    GfxCoord*         secondCoord;
    GfxCoord*         firstCoord;
    s32               offset0;
    s32               offset1;
    s32               offset2;
    s32               offset3;
    s32               halfX;
    s32               halfZ;
    GfxCoord*         coords;
    GfxCoord*         view;
    POLY_FT4*         poly;

    coords      = actor->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorBeamScratch*)SCRATCH_PUSH_BYTES(sizeof(ActorBeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy   = height;
        s->second.vy  = height;
        s->first.vx   = s->firstMatrix.t[0];
        s->first.vz   = s->firstMatrix.t[2];
        s->second.vx  = s->secondMatrix.t[0];
        s->second.vz  = s->secondMatrix.t[2];
        angle         = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX         = (s->first.vx - s->second.vx) / 2;
        halfZ         = (s->first.vz - s->second.vz) / 2;
        offset0       = rcos(angle) * width;
        s->corner0.vy = height;
        s->corner0.vx = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1       = rcos(angle) * width;
        s->corner1.vy = height;
        s->corner1.vx = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2       = rcos(angle) * width;
        s->corner2.vy = height;
        s->corner2.vx = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3       = rcos(angle) * width;
        s->corner3.vy = height;
        s->corner3.vx = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `gGfxViewCoord.workm`, whose high half the GTE loads below share. */
        view               = &gGfxViewCoord;
        view->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(view);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code            = 0x2E;
            PRIM_XY_WORD(poly, 0) = s->screen0;
            PRIM_XY_WORD(poly, 1) = s->screen1;
            PRIM_XY_WORD(poly, 2) = s->screen2;
            PRIM_XY_WORD(poly, 3) = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorBeamScratch));
    }
}
