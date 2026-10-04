/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Draws a flat textured quad at height `height` spanning the model parts
/// `firstJoint` and `secondJoint`, `width` wide on each side of the line
/// between them, tinted grey by `shade`. The working set lives in a frame
/// carved off the scratchpad and released again; nothing is drawn when the
/// two parts are the same or the quad is off screen.
void madChaserDrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade)
{
    MadChaserLimbShadowScratch* s;
    s16                         angle;
    GfxCoord*                   secondCoord;
    GfxCoord*                   firstCoord;
    s32                         offset0;
    s32                         offset1;
    s32                         offset2;
    s32                         offset3;
    GfxCoord*                   coords;
    POLY_FT4*                   poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = SCRATCH_STACK_RESERVE_BLOCK(MadChaserLimbShadowScratch);
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->firstPos.vy             = (s16)height;
        s->secondPos.vy            = (s16)height;
        s->firstPos.vx             = s->firstMatrix.t[0];
        s->firstPos.vz             = s->firstMatrix.t[2];
        s->secondPos.vx            = s->secondMatrix.t[0];
        s->secondPos.vz            = s->secondMatrix.t[2];
        angle                      = ratan2(s->secondPos.vx - s->firstPos.vx, s->secondPos.vz - s->firstPos.vz);
        s->halfSpanX               = (s->firstPos.vx - s->secondPos.vx) / 2;
        s->halfSpanZ               = (s->firstPos.vz - s->secondPos.vz) / 2;
        offset0                    = rcos(angle) * width;
        s->corners[0].vy           = (s16)height;
        s->corners[0].vx           = s->halfSpanX + (s->firstPos.vx - (offset0 >> 0xC));
        s->corners[0].vz           = s->halfSpanZ + (s->firstPos.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1                    = rcos(angle) * width;
        s->corners[1].vy           = (s16)height;
        s->corners[1].vx           = s->halfSpanX + (s->firstPos.vx + (offset1 >> 0xC));
        s->corners[1].vz           = s->halfSpanZ + (s->firstPos.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2                    = rcos(angle) * width;
        s->corners[2].vy           = (s16)height;
        s->corners[2].vx           = (s->secondPos.vx - (offset2 >> 0xC)) - s->halfSpanX;
        s->corners[2].vz           = (s->secondPos.vz + ((s32)(rsin(angle) * width) >> 0xC)) - s->halfSpanZ;
        offset3                    = rcos(angle) * width;
        s->corners[3].vy           = (s16)height;
        s->corners[3].vx           = (s->secondPos.vx + (offset3 >> 0xC)) - s->halfSpanX;
        s->corners[3].vz           = (s->secondPos.vz - ((s32)(rsin(angle) * width) >> 0xC)) - s->halfSpanZ;
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
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
        SCRATCH_STACK_RELEASE_BLOCK(MadChaserLimbShadowScratch);
    }
}
