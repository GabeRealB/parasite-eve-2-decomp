/* Part of the red beacon library; see red_beacon.h. */

/// Draws one frame of a pair of red-shaded gradient quads and then retires
/// the task. The task coordinate's origin is projected once through
/// `GsWSMATRIX` (`RTPS`) into a 0x14-byte scratch stack block; anything
/// nearer than `otz` 0x11 is not drawn. The red level pulses with the global
/// counter `gDisplayState.animFrame` times `spawnArg1`'s low byte, folded into a 0..0x80
/// triangle; `spawnArg1`'s second byte sets the quads' extent, divided by
/// `otz` so they shrink with distance.
void redBeaconTask(Task* arg0)
{
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    GfxCoord*         coord;
    void*             mem;
    s32               i;
    s32               red;
    s32               pulse;
    s32               level;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 0x14;
    block                      = (RoomShaftScratch*)(head - 0x14);
    block->vec.vx              = (u16)coord->workm.t[0];
    block->vec.vy              = (u16)coord->workm.t[1];
    block->vec.vz              = (u16)coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        pulse = gDisplayState.animFrame * ((RoomShaftArg*)&arg0->spawnArg1.value)->phase;
        if (pulse & 0x80) {
            level = 0x80 - (pulse & 0x7F);
        } else {
            level = pulse & 0x7F;
        }
        red              = level;
        block->halfWidth = (((RoomShaftArg*)&arg0->spawnArg1.value)->height << 9) / block->otz;
        for (i = 0; i < 2; i++) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            prim->y1                       = (block->sy - block->halfWidth) + block->halfWidth * (i + i);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x14);
    Gp_ReleaseState1CMem(mem, arg0);
}
