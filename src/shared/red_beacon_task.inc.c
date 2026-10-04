/* Part of the red beacon library; see red_beacon.h. */

/// Draws one frame of a pair of red-shaded gradient quads and then retires
/// the task. The task coordinate's origin is projected once through
/// `GsWSMATRIX` (`RTPS`) into a `RoomGlowSpriteScratch` block; anything
/// nearer than `otz` 0x11 is not drawn. `spawnArg1` is a `RedBeaconArg`: the
/// red level pulses with the global counter `gDisplayState.animFrame` times
/// its `pulseRate`, folded into a 0..0x80 triangle, and its `size` sets the
/// quads' extent, divided by `otz` so they shrink with distance.
void redBeaconTask(Task* arg0)
{
    RoomGlowSpriteScratch* block;
    POLY_G4*               prim;
    GfxCoord*              coord;
    void*                  mem;
    s32                    i;
    s32                    red;
    s32                    pulse;
    s32                    level;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    block              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    block->worldPos.vx = coord->workm.t[0];
    block->worldPos.vy = coord->workm.t[1];
    block->worldPos.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        pulse = gDisplayState.animFrame * ((RedBeaconArg*)&arg0->spawnArg1)->pulseRate;
        if (pulse & 0x80) {
            level = 0x80 - (pulse & 0x7F);
        } else {
            level = pulse & 0x7F;
        }
        red               = level;
        block->halfExtent = (((RedBeaconArg*)&arg0->spawnArg1)->size << 9) / block->otz;
        for (i = 0; i < 2; i++) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx - block->halfExtent;
            prim->x1 = prim->x2 = block->screenPos.vx;
            prim->x3            = block->screenPos.vx + block->halfExtent;
            prim->y0 = prim->y2 = prim->y3 = block->screenPos.vy;
            prim->y1                       = (block->screenPos.vy - block->halfExtent) + block->halfExtent * (i + i);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(mem, arg0);
}
