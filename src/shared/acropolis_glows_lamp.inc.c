/* Part of the Acropolis glows library; see acropolis_glows.h. */

/// Glow sprite task: queues one camera-facing, semi-transparent `POLY_FT4`
/// centred on the task's coordinate frame. The frame's translation is
/// projected through `GsWSMATRIX` into a `RoomGlowSpriteScratch` block, and
/// the quad is a square of half-extent `0x6180 / otz` around the projected
/// point, so it shrinks with distance; nothing is drawn at `otz` 0x10 or less.
///
/// `Task::spawnArg1` (0..2) selects the 0x27x0x27 texture cell at
/// `u = (arg + 1) * 0x28`, `v = 0x10` on tpage 0x2B, the clut
/// `0x4380 | ((arg + 2) & 0x3F)`, and the grey level: a base of
/// 0x20 / 0x60 / 0x20, plus 0x08 / 0x10 / 0x0C on odd
/// `gDisplayState.animFrame`s.
///
/// The work block in `spawnArg2` is released after the quad is queued, so
/// each spawn draws a single frame.
void ACROPOLIS_GLOWS_LAMP_TASK(Task* task)
{
    GfxCoord*              coord;
    EffectWork*            work;
    RoomGlowSpriteScratch* blk;
    POLY_FT4*              prim;
    s32                    grey;
    s32                    clut;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    blk              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    blk->worldPos.vx = coord->workm.t[0];
    blk->worldPos.vy = coord->workm.t[1];
    blk->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->worldPos);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->screenPos);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        u8 base[3] = { 0x20, 0x60, 0x20 };
        u8 step[3] = { 0x08, 0x10, 0x0C };

        grey        = base[task->spawnArg1.value] + (gDisplayState.animFrame & 1) * step[task->spawnArg1.value];
        prim->code |= 2;
        prim->tpage = 0x2B;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        // Assigning through an `s32` keeps the load of `spawnArg1` in SImode;
        // storing the expression straight into the `u16` field lets the front
        // end shorten the whole chain and the load becomes an `lhu`.
        clut            = ((task->spawnArg1.value + 2) & 0x3F) | 0x4380;
        prim->clut      = clut;
        prim->u0        = (task->spawnArg1.value + 1) * 0x28;
        prim->v0        = 0x10;
        prim->u1        = (task->spawnArg1.value + 1) * 0x28 + 0x27;
        prim->v1        = 0x10;
        prim->u2        = (task->spawnArg1.value + 1) * 0x28;
        prim->v2        = 0x37;
        prim->u3        = (task->spawnArg1.value + 1) * 0x28 + 0x27;
        prim->v3        = 0x37;
        blk->halfExtent = 0x6180 / blk->otz;
        prim->x0 = prim->x2 = blk->screenPos.vx - blk->halfExtent;
        prim->x1 = prim->x3 = blk->screenPos.vx + blk->halfExtent;
        prim->y0 = prim->y1 = blk->screenPos.vy - blk->halfExtent;
        prim->y2 = prim->y3 = blk->screenPos.vy + blk->halfExtent;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(work, task);
}

#undef ACROPOLIS_GLOWS_LAMP_TASK
