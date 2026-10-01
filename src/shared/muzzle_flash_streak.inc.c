/* Part of the muzzle flash library; see muzzle_flash.h. */

/// Draws one streak of a gun's muzzle flash as a Gouraud quad: three corners on a 0x100
/// circle around `arg1` (at `-0xC0`, `0`, `+0xC0`) and one tip 0x600 out and
/// 0x200 towards the camera, all in the muzzle coordinate's frame. `arg2` is
/// the flash brightness; only the corner along `arg1` is lit, with half of
/// `arg2` in red and green and all of it in blue.
void muzzleFlashDrawStreak(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                  head;
    MuzzleStreakScratch* blk;
    POLY_G4*             prim;
    MATRIX*              wm;
    s32                  ang;
    s32                  back;
    s32                  len;
    s32                  depth;

    /* `len` and `depth` are locals rather than literals on purpose: as
       constants GCC turns the `* 0x600` into a shift-and-add and drops the
       `mult` the ROM keeps. */
    depth                                     = -0x200;
    head                                      = SCRATCH_STACK_CURSOR(u8);
    blk                                       = (MuzzleStreakScratch*)(head - sizeof(MuzzleStreakScratch));
    SCRATCH_STACK_CURSOR(MuzzleStreakScratch) = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    ang = arg1;

    /* Corner 0: on the small circle, 0xC0 behind the flash direction. */
    back         = ang - 0xC0;
    blk->v[0].vx = (u32)rsin(back) >> 4;
    blk->v[0].vy = (u32)rcos(back) >> 4;
    blk->v[0].vz = 0;
    wm           = &arg0->workm;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((MuzzleStreakScratch*)(head - 0x24))->v[0]);
    gte_rtv0();
    gte_stsv(&((MuzzleStreakScratch*)(head - 0x24))->v[0]);
    (u16) blk->v[0].vx = (u16)blk->v[0].vx + (u16)arg0->workm.t[0];
    (u16) blk->v[0].vy = (u16)blk->v[0].vy + (u16)arg0->workm.t[1];
    (u16) blk->v[0].vz = (u16)blk->v[0].vz + (u16)arg0->workm.t[2];

    /* Corner 1: the far tip, a full 0x600 out and 0x200 towards the camera. */
    len          = 0x600;
    blk->v[1].vx = (rsin(ang) * len) >> 12;
    blk->v[1].vy = (rcos(ang) * len) >> 12;
    blk->v[1].vz = depth;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((MuzzleStreakScratch*)(head - 0x24))->v[1]);
    gte_rtv0();
    gte_stsv(&((MuzzleStreakScratch*)(head - 0x24))->v[1]);
    (u16) blk->v[1].vx = (u16)blk->v[1].vx + (u16)arg0->workm.t[0];
    (u16) blk->v[1].vy = (u16)blk->v[1].vy + (u16)arg0->workm.t[1];
    (u16) blk->v[1].vz = (u16)blk->v[1].vz + (u16)arg0->workm.t[2];

    /* Corner 2: on the small circle, straight along the flash direction. This
       is the only lit corner. */
    blk->v[2].vx = (u32)rsin(ang) >> 4;
    blk->v[2].vy = (u32)rcos(ang) >> 4;
    blk->v[2].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((MuzzleStreakScratch*)(head - 0x24))->v[2]);
    gte_rtv0();
    gte_stsv(&((MuzzleStreakScratch*)(head - 0x24))->v[2]);
    (u16) blk->v[2].vx = (u16)blk->v[2].vx + (u16)arg0->workm.t[0];
    ang                = ang + 0xC0;
    (u16) blk->v[2].vy = (u16)blk->v[2].vy + (u16)arg0->workm.t[1];
    (u16) blk->v[2].vz = (u16)blk->v[2].vz + (u16)arg0->workm.t[2];

    /* Corner 3: on the small circle, 0xC0 ahead of the flash direction. */
    blk->v[3].vx = (u32)rsin(ang) >> 4;
    blk->v[3].vy = (u32)rcos(ang) >> 4;
    blk->v[3].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((MuzzleStreakScratch*)(head - 0x24))->v[3]);
    gte_rtv0();
    gte_stsv(&((MuzzleStreakScratch*)(head - 0x24))->v[3]);
    (u16) blk->v[3].vx = (u16)blk->v[3].vx + (u16)arg0->workm.t[0];
    (u16) blk->v[3].vy = (u16)blk->v[3].vy + (u16)arg0->workm.t[1];
    (u16) blk->v[3].vz = (u16)blk->v[3].vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((MuzzleStreakScratch*)(head - 0x24))->v[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&((MuzzleStreakScratch*)(head - 0x24))->v[1], &((MuzzleStreakScratch*)(head - 0x24))->v[2],
             &((MuzzleStreakScratch*)(head - 0x24))->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (((MuzzleStreakScratch*)(head - 0x24))->otz >= 0x11) {
        setRGB0(prim, 0, 0, 0);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, arg2 >> 1, arg2 >> 1, arg2);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((MuzzleStreakScratch*)(head - 0x24))->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, ((MuzzleStreakScratch*)(head - 0x24))->otz);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(MuzzleStreakScratch));
}
