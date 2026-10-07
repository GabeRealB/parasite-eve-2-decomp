/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Steering round an obstacle on clip 7: closes in (state 0x22) once its
/// spheres stop steering it, or once the player is within
/// DESERT_CHASER_CLOSE_IN.
void desertChaserSteer(Task* arg0)
{
    SVECTOR           delta;
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                                                     = arg0->spawnArg2.pointer;
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_RESET;
#if DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        work->blendActive                                    = 0;
        work->animId                                         = 7;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        _desertChaserAnimTick(arg0);
    }
    _desertChaserAnimTick(arg0);
    if (((s16)_actorContactApplyAvoidancePushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &delta) != 0) ||
        ((s16)_actorContactApplyAvoidancePushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts), &delta) != 0)) {
        work->state = 0x22;
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, arg0->extra.tmd->coords, &delta);
    if (!actorOutsideRadius(&delta, DESERT_CHASER_CLOSE_IN)) {
        work->state = 0x22;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}
