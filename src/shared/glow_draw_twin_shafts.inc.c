/* Part of the glow drawing library; see glow_draw.h. */

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `gSaloonLightPoints`;
/// each root's tip lies at four times its offset to a later entry (15 and 18
/// for the first shaft, 16 and 19 for the second). All four corners are
/// moved to world space through `coord->workm` and projected through
/// `GsWSMATRIX`. The roots take a grey of 0x20 or 0x30 depending on the
/// parity of `gDisplayState.animFrame` and the tips are black, so the shaft
/// fades outward. The quad is sorted by `tipB`'s `otz` and skipped when that
/// is below 0x11.
void glowDrawTwinShafts(GfxCoord* coord)
{
    u8*                    head;
    RoomLightShaftScratch* block;
    POLY_G4*               prim;
    SVECTOR*               dirA;
    SVECTOR*               dirB;
    s32                    i;
    s32                    j;
    s32                    rgb;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x24;
        *scratch = tmp;
        block    = (RoomLightShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gSaloonLightPoints[14]);
    gte_rtv0();
    gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->rootA);
    (u16) block->rootA.vx = (u16)block->rootA.vx + (u16)coord->workm.t[0];
    (u16) block->rootA.vy = (u16)block->rootA.vy + (u16)coord->workm.t[1];
    (u16) block->rootA.vz = (u16)block->rootA.vz + (u16)coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&gSaloonLightPoints[17]);
    gte_rtv0();
    gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->rootB);
    (u16) block->rootB.vx = (u16)block->rootB.vx + (u16)coord->workm.t[0];
    (u16) block->rootB.vy = (u16)block->rootB.vy + (u16)coord->workm.t[1];
    (u16) block->rootB.vz = (u16)block->rootB.vz + (u16)coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                    = i + 15;
        dirA                 = &gSaloonLightPoints[j];
        (u16) block->tipA.vx = (u16)gSaloonLightPoints[14].vx +
                               ((u16)dirA->vx - (u16)gSaloonLightPoints[14].vx) * 4;
        (u16) block->tipA.vy = (u16)gSaloonLightPoints[14].vy +
                               ((u16)dirA->vy - (u16)gSaloonLightPoints[14].vy) * 4;
        (u16) block->tipA.vz = (u16)gSaloonLightPoints[14].vz +
                               ((u16)dirA->vz - (u16)gSaloonLightPoints[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((RoomLightShaftScratch*)(head - 0x24))->tipA);
        gte_rtv0();
        gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->tipA);
        (u16) block->tipA.vx = (u16)block->tipA.vx + (u16)coord->workm.t[0];
        (u16) block->tipA.vy = (u16)block->tipA.vy + (u16)coord->workm.t[1];
        (u16) block->tipA.vz = (u16)block->tipA.vz + (u16)coord->workm.t[2];

        j                    = i + 18;
        dirB                 = &gSaloonLightPoints[j];
        (u16) block->tipB.vx = (u16)gSaloonLightPoints[17].vx +
                               ((u16)dirB->vx - (u16)gSaloonLightPoints[17].vx) * 4;
        (u16) block->tipB.vy = (u16)gSaloonLightPoints[17].vy +
                               ((u16)dirB->vy - (u16)gSaloonLightPoints[17].vy) * 4;
        (u16) block->tipB.vz = (u16)gSaloonLightPoints[17].vz +
                               ((u16)dirB->vz - (u16)gSaloonLightPoints[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((RoomLightShaftScratch*)(head - 0x24))->tipB);
        gte_rtv0();
        gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->tipB);
        (u16) block->tipB.vx = (u16)block->tipB.vx + (u16)coord->workm.t[0];
        (u16) block->tipB.vy = (u16)block->tipB.vy + (u16)coord->workm.t[1];
        (u16) block->tipB.vz = (u16)block->tipB.vz + (u16)coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->rootA);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->rootB, &((RoomLightShaftScratch*)(head - 0x24))->tipA,
                 &((RoomLightShaftScratch*)(head - 0x24))->tipB);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            rgb = ((u8)gDisplayState.animFrame & 1) * 16 + 0x20;
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            setRGB0(prim, rgb, rgb, rgb);
            setRGB1(prim, rgb, rgb, rgb);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x24);
}
