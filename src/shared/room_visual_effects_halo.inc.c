/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (depth + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void RoomFx_DrawMote(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    DisplayState*        ds;
    u16                  row;
    u16                  pal;
    s32                  u0;
    s32                  u1;
    s16                  xy;

    row                  = arg2 >> 12;
    arg2                &= 0xFFF;
    pal                  = arg3 >> 12;
    arg3                &= 0xFF;
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
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->screenExtent = arg2 * 23 / block->depth;
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
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
static void RoomFx_DrawHaloRing(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomFxRadialScratch* block;
    POLY_G4*             prim;
    s32                  ang;
    s32                  next;
    s32                  outer;

    block                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxRadialScratch);
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
        block->radii.ring.black = ((s16)arg1 * 64) / block->depth;
        block->radii.ring.tint  = ((s16)outer * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->screenX + ((block->radii.ring.black * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->radii.ring.black * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->screenX + ((block->radii.ring.black * rsin(next)) >> 12);
            prim->y1 = block->screenY + ((block->radii.ring.black * rcos(next)) >> 12);
            prim->x2 = block->screenX + ((block->radii.ring.tint * rsin(ang)) >> 12);
            prim->y2 = block->screenY + ((block->radii.ring.tint * rcos(ang)) >> 12);
            prim->x3 = block->screenX + ((block->radii.ring.tint * rsin(next)) >> 12);
            prim->y3 = block->screenY + ((block->radii.ring.tint * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxRadialScratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
static void RoomFx_DrawHaloDisc(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFxFanScratch* block;
    POLY_G4*          prim;
    s32               ang;
    s32               depth;

    block                = SCRATCH_STACK_RESERVE_BLOCK(RoomFxFanScratch);
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
        depth         = block->depth + 1;
        block->depth  = depth;
        block->radius = (arg1 * 64) / depth;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->screenY + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFxFanScratch);
}

/// An expanding halo. The first tick parents the effect frame to its anchor
/// at the spawn position and splits the spawn argument into a palette index
/// and a frame count. While the count runs down, the level and the angle grow
/// by 0x100 / count each tick, drawing the halo (plus a half-bright echo on odd
/// ticks) tinted by the palette and a black-edged ring shrinking in from 0x300.
/// It then fades from full level through the afterglow, 0x10 a tick, and
/// releases its work block. It pauses while the room's event state is set and
/// releases the block when that state reaches 4.
static inline void RoomFx_HaloTask(Task* arg0)
{
    u8                rgb[3];
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s16               flag;
    s32               shift;

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
        switch (arg0->state) {
            case 0:
                rot                 = (GfxRotationWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00M01         = ONE;
                rot->m02M10         = 0;
                rot->m11M12         = ONE;
                rot->m20M21         = 0;
                rot->m22            = ONE;
                coord->coord.t[0]   = mem->pos.vx;
                coord->coord.t[1]   = mem->pos.vy;
                coord->coord.t[2]   = mem->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                shift                 = arg0->spawnArg1.halves.high;
                mem->index            = shift;
                arg0->spawnArg1.value = arg0->spawnArg1.halves.low;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                actorRenderComposeCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> RoomFx_GetHaloShades()[mem->index].rShift;
                rgb[1]                 = mem->scale >> RoomFx_GetHaloShades()[mem->index].gShift;
                rgb[2]                 = mem->scale >> RoomFx_GetHaloShades()[mem->index].bShift;
                RoomFx_DrawHaloDisc(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    RoomFx_DrawHaloDisc(coord, (s16)(mem->angle + 0x100), rgb);
                }
                RoomFx_DrawHaloRing(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                actorRenderComposeCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> RoomFx_GetHaloShades()[mem->index].rShift;
                    rgb[1] = mem->scale >> RoomFx_GetHaloShades()[mem->index].gShift;
                    rgb[2] = mem->scale >> RoomFx_GetHaloShades()[mem->index].bShift;
                    RoomFx_DrawFlashStar(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    effectKillTask(mem, arg0);
}

/// A burst in orange. Each tick draws a disc and a glow at a growing size while
/// a wider, dimmer ring expands and fades behind them; once that ring is gone
/// the main level falls 0x18 a tick and the work block is released. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
static inline void RoomFx_OrangeBurstTask(Task* arg0)
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
        RoomFx_DrawHaloDisc(coord, (s16)(step * 2), rgb);
        RoomFx_DrawBurstGlow(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            RoomFx_DrawHaloRing(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
