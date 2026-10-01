/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Links the actor's three collision
/// objects onto `Gp_ObjLists[2]` and clears their record tables.
void madChaserLinkBodies(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->obj_2AC.coord            = &arg0->extra.tmd->coords[1];
    work->obj_2AC.context.contacts = work->rec_2EC;
    work->obj_2AC.pos.vx           = 0;
    work->obj_2AC.pos.vy           = 0;
    work->obj_2AC.pos.vz           = 0;
    work->obj_2AC.key              = 0x3002C;
    work->obj_2AC.radius           = 0x170;
    work->obj_2AC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_2AC);
    Gp_InitRec18Table(work->rec_2EC, 8, 0);
    work->obj_2AC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->obj_3AC.coord            = &arg0->extra.tmd->coords[1];
    work->obj_3AC.context.contacts = work->rec_3CC;
    work->obj_3AC.pos.vx           = 0;
    work->obj_3AC.pos.vy           = 0;
    work->obj_3AC.pos.vz           = 0;
    work->obj_3AC.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj_3AC.radius           = 0x170;
    work->obj_3AC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_3AC);
    Gp_InitRec18Table(work->rec_3CC, 2, 0);
    work->obj_3AC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj_2CC.coord            = &arg0->extra.tmd->coords[1];
    work->obj_2CC.context.contacts = work->rec_2EC;
    work->obj_2CC.pos.vx           = 0;
    work->obj_2CC.pos.vy           = 0;
    work->obj_2CC.pos.vz           = 0;
    work->obj_2CC.key              = 0x3002C;
    work->obj_2CC.radius           = 0x224;
    work->obj_2CC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_2CC);
    work->obj_2CC.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}
