/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn-entry body, as in the Horned Stranger's `func_actor_401300_80137D78`: carve the
/// aim scratch off the scratch stack, and while the live-actor flag is up reset
/// the display nodes and rebuild the actor's facing. The turn direction
/// (`field_C08`) is drawn from `gRandomLcgState` on the first entry, and each entry
/// swings the facing toward the player by `field_C12` plus a 0x171 bias until
/// `field_C26` has been counted once. The forward direction `field_BF0` comes
/// out of the turn angle through `gfxRotMatrixY`, and the `field_C0A` draw
/// scales it onto the scratch vector; the actor is then slid along its obstacle
/// table, halving that draw while it overlaps. Counts the entry in `field_6`
/// and keys state 7 once 0x1E of them have run.
void oddStrangerSidestep(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    ActorChaseScratch* head;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_SWING_RADIUS;
        work->field_6          = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C08 == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C08 = 1;
            } else {
                work->field_C08 = -1;
            }
        }
        if (work->field_C08 == 1) {
            work->field_89E = 0x15;
            if (work->field_C26 == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->field_C12 + angle;
            } else {
                aim->turn += work->field_C12;
            }
            work->field_C08 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C26 == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->field_C12;
            } else {
                aim->turn -= work->field_C12;
            }
            work->field_C08 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        oddStrangerDrive(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->field_BF0;
        gfxReadMatrixZAxis(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C0A = 0xDE;
        work->field_C26++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C0A);
        gte_ldsv(&work->field_BF0);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C0A >> 1);
        gte_ldsv(&work->field_BF0);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1) {
            work->field_C0A = (u16)(work->field_C0A >> 1);
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
