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
    if (work->field_4 != 0) {
        enemy                                                     = arg0->spawnArg2.pointer;
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].body.radius = 0x19C;
        work->field_828           = 2;
#if DESERT_CHASER_RUN_SEQUENCE
        work->field_832 = DESERT_CHASER_SLOT_RATE(work);
#endif
        work->field_82A           = 0;
        work->field_82E           = 7;
        work->objs[2].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->field_832 = DESERT_CHASER_SLOT_RATE(work);
#endif
        desertChaserAnimTick(arg0);
    }
    desertChaserAnimTick(arg0);
    if (((s16)ActorContact_Steer(arg0->extra.tmd->coords, work->objs[0].contacts, ARRAY_SIZE(work->objs[0].contacts), &delta) != 0) ||
        ((s16)ActorContact_Steer(arg0->extra.tmd->coords, work->objs[1].contacts, ARRAY_SIZE(work->objs[1].contacts), &delta) != 0)) {
        work->field_0 = 0x22;
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &delta);
    if (!actorOutsideRadius(&delta, DESERT_CHASER_CLOSE_IN)) {
        work->field_0 = 0x22;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}
