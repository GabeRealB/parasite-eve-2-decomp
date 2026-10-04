/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void RoomFx_DrawFlyingSpark(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**               scratch;
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    DisplayState*        ds;
    s32                  tex;
    s32                  sarg;
    s32                  t;
    s16                  xy;
    u16                  vz;

    tex                                                                         = arg1;
    scratch                                                                     = SCRATCH_STACK_CURSOR_SLOT;
    head                                                                        = *scratch;
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = arg0->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = arg0->workm.t[1];
    vz                                                                          = arg0->workm.t[2];
    *scratch                                                                    = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg                = (s16)arg2;
        t                   = sarg * 24;
        block->screenExtent = (t - sarg) / block->depth;
        xy                  = block->screenX - block->screenExtent;
        prim->x2            = xy;
        prim->x0            = xy;
        xy                  = block->screenX + block->screenExtent;
        prim->x3            = xy;
        prim->x1            = xy;
        xy                  = block->screenY - block->screenExtent;
        prim->y1            = xy;
        prim->y0            = xy;
        xy                  = block->screenY + block->screenExtent;
        prim->y3            = xy;
        prim->y2            = xy;
        ds                  = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(EffectCentreScratch));
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows. The
/// same drawing as `RoomFx_DrawHaloRing`, with
/// its scratch block laid out differently.
static void RoomFx_DrawFlyingRing(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    EffectShapeScratch* block;
    POLY_G4*            prim;
    s32                 ang;
    s32                 next;
    s32                 outer;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    outer                = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->extent.ring.inner = ((s16)arg1 * 64) / block->depth;
        block->extent.ring.outer = ((s16)outer * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->screenX + ((block->extent.ring.inner * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->extent.ring.inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->screenX + ((block->extent.ring.inner * rsin(next)) >> 12);
            prim->y1 = block->screenY + ((block->extent.ring.inner * rcos(next)) >> 12);
            prim->x2 = block->screenX + ((block->extent.ring.outer * rsin(ang)) >> 12);
            prim->y2 = block->screenY + ((block->extent.ring.outer * rcos(ang)) >> 12);
            prim->x3 = block->screenX + ((block->extent.ring.outer * rsin(next)) >> 12);
            prim->y3 = block->screenY + ((block->extent.ring.outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
static void RoomFx_DrawFlyingDisc(GfxCoord* arg0, s32 arg1, u8* rgb)
{
    EffectCentreScratch* block;
    POLY_G4*             prim;
    s32                  ang;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->screenExtent = ((s16)arg1 * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->screenExtent * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->screenExtent * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->screenExtent * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->screenY + ((block->screenExtent * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// A burst in orange, the same effect as
/// `RoomFx_OrangeBurstTask` drawn with this unit's
/// copies of its helpers. Each tick draws a disc and a glow at a growing size
/// while a wider, dimmer ring expands and fades behind them; once that ring is
/// gone the main level falls 0x18 a tick and the work block is released. It
/// pauses while the room's event state is set and releases the block when that
/// state reaches 4.
static inline void RoomFx_OrangeBurst2Task(Task* arg0)
{
    u8          rgb[3];
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         step;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        actorRenderComposeCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        RoomFx_DrawFlyingDisc(coord, (s16)(step * 2), rgb);
        RoomFx_DrawBurst2Glow(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            RoomFx_DrawFlyingRing(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            effectKillTask(mem, arg0);
        }
    }
}
