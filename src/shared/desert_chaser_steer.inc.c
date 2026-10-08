/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Plays the avoidance clip and resumes pursuit on player-body contact or proximity.
///
/// Requires armed work, enemy/model and populated contact arrays. Either sphere's
/// avoidance call reporting player or companion contact selects resume-pursuit;
/// that result is independent of whether it displaced the chaser. A separate
/// horizontal range test uses the build's close-in distance in root-parent units.
/// The low-halfword return tests are retained; no scratch allocation or work is owned.
static void _desertChaserSteer(Task* task)
{
    SVECTOR           playerOffset;
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_RESET;
#if DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        work->blendActive                                    = 0;
        work->animId                                         = DESERT_CHASER_CLIP_STEER;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        _desertChaserAnimTick(task);
    }
    _desertChaserAnimTick(task);
    if (((s16)_actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &playerOffset) != 0) ||
        ((s16)_actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts), &playerOffset) != 0)) {
        work->state = DESERT_CHASER_STATE_RESUME_PURSUIT;
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &playerOffset);
    if (!actorOutsideRadius(&playerOffset, DESERT_CHASER_CLOSE_IN)) {
        work->state = DESERT_CHASER_STATE_RESUME_PURSUIT;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}
