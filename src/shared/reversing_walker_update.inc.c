/* Part of the reversing walker library; see reversing_walker.h. */

/// the counter at `walk.motion` indexes, then integrates the local-space `velocity`
/// into the 16.16 accumulators at `walk.carry`, adds their high halves to the
/// root coordinate's translation and truncates them back to 16 bits. Ticks the
/// animation slots while `model.ticking` is set, and -- unless the display object's
/// `flags` carry 0x80 -- draws the ground-shadow quad from the second
/// part's world matrix, clears that part's `composeStamp` and rebuilds its coordinate.
/// The `freeCountdown` countdown then runs while it is non-negative, freeing the
/// model buffers on the frame it reaches zero; the init's -1 disables it.
void reverseWalkUpdate(Task* arg0)
{
    TmdObject*       ext      = arg0->extra.tmd;
    Actor350500Work* work     = (Actor350500Work*)arg0->work;
    TaskFunc         funcs[2] = { reverseWalkIdle, reverseWalkRunStep };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    funcs[work->walk.motion](arg0);
    coord                     = arg0->extra.tmd->coords;
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    coord->coord.t[0]        += work->walk.carry[0].halves.integer;
    coord->coord.t[1]        += work->walk.carry[1].halves.integer;
    coord->coord.t[2]        += work->walk.carry[2].halves.integer;
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}
