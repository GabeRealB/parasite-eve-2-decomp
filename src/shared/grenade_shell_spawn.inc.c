/* Part of the grenade shell library; see grenade_shell.h. */

/// Spawn state of the projectile: allocates the 0xA0 `WeaponGrenadeWork` block,
/// places the projectile at the per-ammo muzzle offset from `gGrenadeShellMuzzleOffsets`, parents it
/// to the world coordinate, sets its launch speed from `gGrenadeShellSpeeds` and links its two
/// collision nodes. The Grenade Pistol and MM1 share it by being one source.
void grenadeShellSpawn(Task* arg0)
{
    u8*                head;
    SVECTOR*           blk;
    SVECTOR*           vec;
    MATRIX*            mtx;
    TmdObject*         extra;
    GfxCoord*          coord;
    GfxCoord*          muzzle;
    WeaponGrenadeWork* work;
    s32                idx;
    s32                flags;
    s32                speed;

    head                          = SCRATCH_STACK_CURSOR(u8);
    blk                           = (SVECTOR*)(head - 8);
    SCRATCH_STACK_CURSOR(SVECTOR) = blk;
    extra                         = arg0->extra.tmd;
    idx                           = ((u32)arg0->spawnArg1.value >> 16) & 0xF;
    coord                         = extra->coords;
    muzzle                        = coord->parent;
    work                          = memCalloc(sizeof(WeaponGrenadeWork), 0);
    vec                           = blk;
    if (work == NULL) {
        SCRATCH_STACK_RELEASE_BYTES(8);
        taskKill(arg0);
        return;
    }
    arg0->work         = work;
    arg0->exitCallback = grenadeShellExit;
    arg0->state++;
    memFillBytes(work, 0, sizeof(WeaponGrenadeWork));
    blk->vx              = gGrenadeShellMuzzleOffsets[idx].vx;
    blk->vy              = gGrenadeShellMuzzleOffsets[idx].vy;
    blk->vz              = gGrenadeShellMuzzleOffsets[idx].vz;
    muzzle->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr();
    gte_stlvnl(coord->workm.t);
    mtx = &coord->coord;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, mtx);
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    gfxRotMatrixX(mtx, -0x400, GRAPHICS_ROTATION_COMPOSE);
    gfxReadMatrixZAxis(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    speed                             = gGrenadeShellSpeeds[idx];
    work->smokeInterval               = 1;
    work->flightFrame                 = 0;
    work->sphereBody.coord            = coord;
    work->sphereBody.context.contacts = work->sphereContacts;
    work->sphereBody.pos.vx           = 0;
    work->sphereBody.pos.vy           = 0;
    work->sphereBody.pos.vz           = 0;
    work->flightTimer.word            = speed << 16;
    flags                             = (u16)arg0->spawnArg1.value | 0x20000;
    work->sphereBody.key              = flags;
    if (arg0->spawnArg1.value & 0x100000) {
        work->sphereBody.key = flags | 0x80;
    }
    work->sphereBody.radius = 0x94;
    work->sphereBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->sphereBody);
    worldCollisionInitContacts(work->sphereBody.context.contacts, 1, 0);
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->capsule.contacts            = work->capsuleContacts;
    work->capsuleBody.coord           = coord;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = 0;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = 0;
    work->capsuleBody.radius          = 0;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[0].vy          = 0;
    work->capsule.ends[0].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.ends[1].vy          = 0;
    work->capsule.end0Radius          = 1;
    work->capsule.end1Radius          = 1;
    work->sphereBody.flags           |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->capsule.ends[1].vz          = -(work->flightTimer.word >> 10);
    Gp_LinkObj(1, &work->capsuleBody);
    worldCollisionInitContacts(work->capsule.contacts, 1, 0);
    work->capsuleBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    SCRATCH_STACK_RELEASE_BYTES(8);
}
