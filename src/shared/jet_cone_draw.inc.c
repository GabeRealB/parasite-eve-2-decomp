/* Part of the jet cone library; see jet_cone.h. */

static void jetConeDraw(GfxCoord* coord, s16 age, s16 length, s32 shortCone)
{
    GpBandScratch* sc;
    POLY_FT4*      prim;
    SVECTOR*       vert;
    s32            rimRad;
    s32            hubRad;
    s32            rimSize;
    s32            hubSize;
    s32            i;
    s32            next;
    s32            ang;
    s32            u0;
    s16            back;
    MATRIX*        rot;

    /* `rimSize` / `hubSize` are latched into the loop's own `rimRad` /
       `hubRad` on purpose: the ROM keeps the two copies the single pair would
       have coalesced away, and `rot` is a second spelling of `&coord->workm`
       for the same reason. `vert` reaches `outer[i]` (the hub) through `inner[i]` (the rim) rather
       than off `sc`, so the `gte_ldv0` / `gte_stsv` address stays a register
       of its own instead of being shared with the field stores. */
    sc = SCRATCH_STACK_RESERVE_BLOCK(GpBandScratch);
    if (shortCone != 0) {
        back    = (length << 1) + (age << 8);
        hubSize = 0x80;
        rimSize = JET_CONE_RIM_SHORT;
    } else {
        back    = length + (age << 4);
        hubSize = 0x40;
        rimSize = JET_CONE_RIM_LONG;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    i      = 0;
    rimRad = rimSize;
    rot    = &coord->workm;
    hubRad = hubSize;
    for (; i < 0x10; i++) {
        ang             = i << 8;
        sc->inner[i].vx = (rsin(ang) * rimRad) >> 12;
        sc->inner[i].vy = (rcos(ang) * rimRad) >> 12;
        sc->inner[i].vz = -back;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->inner[i]);
        gte_rtv0();
        gte_stsv(&sc->inner[i]);
        sc->inner[i].vx += (u16)coord->workm.t[0];
        sc->inner[i].vy += (u16)coord->workm.t[1];
        sc->inner[i].vz += (u16)coord->workm.t[2];
        sc->outer[i].vx  = (rsin(ang) * hubRad) >> 12;
        vert             = &sc->inner[i] + 16;
        vert->vy         = (rcos(ang) * hubRad) >> 12;
        vert->vz         = 0;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->outer[i]);
        gte_rtv0();
        gte_stsv(&sc->outer[i]);
        sc->outer[i].vx += (u16)coord->workm.t[0];
        vert->vy        += (u16)coord->workm.t[1];
        vert->vz        += (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 0x10; i++) {
        gte_ldv0(&sc->inner[i]);
        gte_rtps();
        gte_stsxy(&sc->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&sc->inner[next], &sc->outer[i], &sc->outer[next]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&sc->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&sc->otz);
            sc->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            prim->tpage = 0x2A;
            prim->clut  = JET_CONE_CLUT;
            setRGB0(prim, 0x30, 0x30, 0x30);
            setSemiTrans(prim, 1);
            u0 = (s16)((JET_CONE_FRAME_JITTER[i] + age) % 6) * 40;
            setUV4(prim, u0, 0x60, u0 + 0x27, 0x60, u0, 0x87, u0 + 0x27, 0x87);
            setXY4(prim, sc->sxy0.vx, sc->sxy0.vy, sc->sxy1.vx, sc->sxy1.vy, sc->sxy2.vx, sc->sxy2.vy, sc->sxy3.vx,
                   sc->sxy3.vy);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpBandScratch);
}
