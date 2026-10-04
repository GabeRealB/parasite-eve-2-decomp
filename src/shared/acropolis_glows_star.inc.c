/* Part of the Acropolis glows library; see acropolis_glows.h. */

// Select the core quad's half-height storage for this included instance.
#if defined(GLOW_STAR_STORE_HALF_HEIGHT) && GLOW_STAR_STORE_HALF_HEIGHT
#define ACROPOLIS_GLOWS_STAR_HALF_HEIGHT blk->cornerDy
#else
#define ACROPOLIS_GLOWS_STAR_HALF_HEIGHT blk->cornerDx
#endif

/// One frame of the promenade's twinkling star: two semi-transparent
/// `POLY_FT4`s stacked on the same screen point, centred on the task's own
/// coordinate frame. The frame's translation is projected through `GsWSMATRIX`
/// into an `OverlaySpriteScratch` block on the scratch stack, and both quads
/// are dropped entirely inside `otz` 0x11.
///
/// The lower quad is upright, of half-extent `0x1680 / otz`, and animates
/// through six 0x10x0x10 cells at v = 0 on tpage 0x2B by stepping `u` with
/// `work->age % 6`; it is drawn `code |= 3`, so semi-transparent *and*
/// unshaded. The upper quad is the 0x27x0x27 flare at v = 0x10 with clut
/// 0x4381, drawn at `0x3A80 / otz` from the centre along the spin angle
/// `work->scale` and its quarter-turn (`+ 0x400`), so it rotates a frame at
/// a time. Its colour is a fresh random grey (0x20..0x7F, equal on all three
/// channels) every frame, which is what makes the star flicker.
///
/// Like the promenade's other glows, the task is one-shot: the work block is
/// released as soon as both quads have been queued, so the room respawns it
/// every frame it wants the star.
void ACROPOLIS_GLOWS_STAR_TASK(Task* task)
{
    GfxCoord*             coord;
    EffectWork*           work;
    void**                scratch;
    OverlaySpriteScratch* head;
    OverlaySpriteScratch* blk;
    s32*                  otzp;
    POLY_FT4*             prim;
    s32                   grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    work->age        = task->spawnArg1.value;
    scratch          = SCRATCH_STACK_CURSOR_SLOT;
    head             = *scratch;
    blk              = head - 1;
    otzp             = &blk->otz;
    blk->worldPos.vx = coord->workm.t[0];
    blk->worldPos.vy = coord->workm.t[1];
    *scratch         = blk;
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
    gte_stszotz(otzp);
    if (blk->otz >= 0x11) {
        prim->tpage   = 0x2B;
        prim->clut    = 0x4380;
        prim->code   |= 3;
        prim->u0      = (work->age % 6) * 16;
        prim->v0      = 0;
        prim->u1      = (work->age % 6) * 16 + 0xF;
        prim->v1      = 0;
        prim->u2      = (work->age % 6) * 16;
        prim->v2      = 0xF;
        prim->u3      = (work->age % 6) * 16 + 0xF;
        prim->v3      = 0xF;
        blk->cornerDx = 0x1680 / blk->otz;
#if defined(GLOW_STAR_STORE_HALF_HEIGHT) && GLOW_STAR_STORE_HALF_HEIGHT
        blk->cornerDy = 0x1680 / blk->otz;
#endif
        prim->x0 = prim->x2 = blk->screenPos.vx - blk->cornerDx;
        prim->x1 = prim->x3 = blk->screenPos.vx + blk->cornerDx;
        prim->y0 = prim->y1 = blk->screenPos.vy - ACROPOLIS_GLOWS_STAR_HALF_HEIGHT;
        prim->y2 = prim->y3 = blk->screenPos.vy + ACROPOLIS_GLOWS_STAR_HALF_HEIGHT;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);

        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->clut      = 0x4381;
        prim->tpage     = 0x2B;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) % 96 + 0x20;
        prim->u0        = 0;
        prim->v0        = 0x10;
        prim->u1        = 0x27;
        prim->v1        = 0x10;
        prim->u2        = 0;
        prim->v2        = 0x37;
        prim->u3        = 0x27;
        prim->v3        = 0x37;
        prim->code     |= 2;
        prim->r0        = grey;
        prim->g0        = grey;
        prim->b0        = grey;

        work->scale   = gDisplayState.animFrame + work->age;
        blk->cornerDx = ((0x3A80 / blk->otz) * rsin(work->scale)) >> 12;
        blk->cornerDy = ((0x3A80 / blk->otz) * rcos(work->scale)) >> 12;
        prim->x0      = blk->screenPos.vx + blk->cornerDx;
        prim->x3      = blk->screenPos.vx - blk->cornerDx;
        prim->y0      = blk->screenPos.vy - blk->cornerDy;
        prim->y3      = blk->screenPos.vy + blk->cornerDy;
        blk->cornerDx = ((0x3A80 / blk->otz) * rsin(work->scale + 0x400)) >> 12;
        blk->cornerDy = ((0x3A80 / blk->otz) * rcos(work->scale + 0x400)) >> 12;
        prim->x1      = blk->screenPos.vx + blk->cornerDx;
        prim->x2      = blk->screenPos.vx - blk->cornerDx;
        prim->y1      = blk->screenPos.vy - blk->cornerDy;
        prim->y2      = blk->screenPos.vy + blk->cornerDy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
    effectKillTask(work, task);
}

#undef ACROPOLIS_GLOWS_STAR_TASK
#undef GLOW_STAR_STORE_HALF_HEIGHT
#undef ACROPOLIS_GLOWS_STAR_HALF_HEIGHT
