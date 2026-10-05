/* Part of the Glutton library; see glutton.h. */

/// Flight step of the seized player's model: carry it along the model's own
/// forward axis until it lands. `stateChanged` (the dispatcher's state-changed
/// flag) re-arms the step counter, the ground marker and the first display
/// node on the frame the state starts.
///
/// While the game is running (`gSceneCombatState.actorControl` clear) the model falls 0xA a step,
/// column 2 of its coordinate is normalised into a scratchpad `SVECTOR` and
/// scaled by 0x89/0x1000 through the GTE's GPF, and that is the per-step
/// translation added to the coordinate; past step 0x29 the height is pinned to
/// -0x3E8 instead. The marker grows 0x60 a step and is drawn under the work
/// block's own coordinate, which is parented to `gGfxViewCoord` and tracks the
/// model. After 0x35 steps the display node is handed back and the task steps
/// on. Paused (`gSceneCombatState.actorControl` set) only the coordinate is refreshed, and the
/// marker is skipped while the host actor sits in state 6.
///
/// Bails out -- unlinking the display node and stepping the task on -- when the
/// overlay is shutting down or the host actor has left the grab states.
void gluttonThrowFly(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    GluttonWork*           host;
    Enemy*                 owner;
    u8*                    head;
    SVECTOR*               dir;
    /// Second live alias of `dir`: the GTE operand is kept in its own register
    /// for the whole function, which is what gives this function its seventh
    /// callee-saved slot.
    SVECTOR* gteDir;

    work  = task->work;
    owner = task->parent->spawnArg2.pointer;
    host  = owner->task->work;

    if (gGluttonEnded == 1 || host->state == 0x10 || host->state == 5 ||
        host->state == 0xC || host->state == 0x12) {
        task->state++;
        worldCollisionUnlinkBody(&work->attackBody);
        return;
    }

    head                          = SCRATCH_STACK_CURSOR(u8);
    dir                           = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(SVECTOR) = dir;
    gteDir                        = dir;

    if (work->stateChanged != 0) {
        work->stateTicks                  = 0;
        work->shadowGrowth                = 0x400;
        work->stateChanged                = 0;
        work->attackContacts[0].key.value = 0;
        work->attackBody.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        work->stateTicks++;
        task->extra.tmd->coords->coord.t[1] += 0xA;

        gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, dir);
        VectorNormalSS(dir, dir);
        gte_lddp(0x89);
        gte_ldsv(gteDir);
        gte_gpf12();
        gte_stsv(gteDir);

        task->extra.tmd->coords->coord.t[0] += dir->vx;
        task->extra.tmd->coords->coord.t[1] += dir->vy;
        if (work->stateTicks >= 0x29) {
            task->extra.tmd->coords->coord.t[1] = -0x3E8;
        }
        task->extra.tmd->coords->coord.t[2]  += dir->vz;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

        work->shadowGrowth += 0x60;
        worldCollisionClearContacts(work->attackContacts);

        work->shadowCoord.parent = &gGfxViewCoord;
        gfxRotMatrixY(&work->shadowCoord.coord, 0, 1);
        work->shadowCoord.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
        work->shadowCoord.coord.t[1]   = 0;
        work->shadowCoord.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
        work->shadowCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->shadowCoord);

        effectDrawGroundShadow(MATRIX_TRANS(&work->shadowCoord.workm), (work->shadowGrowth >> 3) + 0x100,
                               gRoomEffectState->groundShadowShade);

        if (work->stateTicks >= 0x35) {
            worldCollisionUnlinkBody(&work->attackBody);
            task->state++;
            work->stateChanged = 1;
        }
    } else {
        work->shadowCoord.parent = &gGfxViewCoord;
        gfxRotMatrixY(&work->shadowCoord.coord, 0, 1);
        work->shadowCoord.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
        work->shadowCoord.coord.t[1]   = 0;
        work->shadowCoord.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
        work->shadowCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->shadowCoord);

        if (host->phase != 6) {
            effectDrawGroundShadow(MATRIX_TRANS(&work->shadowCoord.workm), (work->shadowGrowth >> 3) + 0x100,
                                   gRoomEffectState->groundShadowShade);
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}
