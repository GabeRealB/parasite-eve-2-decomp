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
    Mem_Set(work, 0, sizeof(WeaponGrenadeWork));
    blk->vx              = gGrenadeShellMuzzleOffsets[idx].vx;
    blk->vy              = gGrenadeShellMuzzleOffsets[idx].vy;
    blk->vz              = gGrenadeShellMuzzleOffsets[idx].vz;
    muzzle->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr();
    gte_stlvnl(coord->workm.t);
    mtx = &coord->coord;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, mtx);
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    Gfx_RotMatrixX(mtx, -0x400, 0);
    Gfx_MatrixCol2(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    speed                      = gGrenadeShellSpeeds[idx];
    work->field_8C             = 1;
    work->field_90             = 0;
    work->obj.coord            = coord;
    work->obj.context.contacts = work->rec0;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->field_88.word        = speed << 16;
    flags                      = (u16)arg0->spawnArg1.value | 0x20000;
    work->obj.key              = flags;
    if (arg0->spawnArg1.value & 0x100000) {
        work->obj.key = flags | 0x80;
    }
    work->obj.radius = 0x94;
    work->obj.flags  = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->obj);
    Gp_InitRec18Table(work->obj.context.contacts, 1, 0);
    work->obj2.context.capsule = &work->d4rec;
    work->obj2.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->d4rec.contacts       = work->rec1;
    work->obj2.coord           = coord;
    work->obj2.pos.vx          = 0;
    work->obj2.pos.vy          = 0;
    work->obj2.pos.vz          = 0;
    work->obj2.key             = 0;
    work->obj2.radius          = 0;
    work->d4rec.ends[0].vx     = 0;
    work->d4rec.ends[0].vy     = 0;
    work->d4rec.ends[0].vz     = 0;
    work->d4rec.ends[1].vx     = 0;
    work->d4rec.ends[1].vy     = 0;
    work->d4rec.end0Radius     = 1;
    work->d4rec.end1Radius     = 1;
    work->obj.flags           |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->d4rec.ends[1].vz     = -(work->field_88.word >> 10);
    Gp_LinkObj(1, &work->obj2);
    Gp_InitRec18Table(work->d4rec.contacts, 1, 0);
    work->obj2.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    SCRATCH_STACK_RELEASE_BYTES(8);
}
