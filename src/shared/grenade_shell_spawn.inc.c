/* Part of the grenade shell library; see grenade_shell.h. */

/// Arms the flight clock and links the projectile's attack and grid-probe bodies.
///
/// Borrows live task/work/root storage. The initial positive flight divisor
/// is in whole units and is stored as Q16.16. Contact arrays belong to work
/// and remain linked until the shell's exit callback unlinks the bodies.
/// The capsule's initial far end is -64 times that divisor in local Z units;
/// its later reach is updated by flight. Both contact arrays are initialized
/// at their complete one-element extents.
static inline void _grenadeShellLinkFlightBodies(Task* task, WeaponGrenadeWork* work,
                                                 GfxCoord* rootCoord, s32 initialFlightDivisor)
{
    enum {
        GRENADE_SHELL_COMPANION_SHOT        = 1 << 20,
        GRENADE_SHELL_ATTACK_COMPANION_BIT  = 1 << 7,
        GRENADE_SHELL_FLIGHT_FRACTION_BITS  = 16,
        GRENADE_SHELL_INITIAL_CAPSULE_SHIFT = 10,
        GRENADE_SHELL_FLIGHT_SPHERE_RADIUS  = 148,
        GRENADE_SHELL_CAPSULE_END_RADIUS    = 1
    };
    s32 attackKey;

    work->smokeInterval               = 1;
    work->flightFrame                 = 0;
    work->sphereBody.coord            = rootCoord;
    work->sphereBody.context.contacts = work->sphereContacts;
    work->sphereBody.pos.vx           = 0;
    work->sphereBody.pos.vy           = 0;
    work->sphereBody.pos.vz           = 0;
    work->flightTimer.word            = initialFlightDivisor << GRENADE_SHELL_FLIGHT_FRACTION_BITS;
    attackKey                         = (u16)task->spawnArg1.value | WORLD_COLLISION_CONTACT_ATTACK;
    work->sphereBody.key              = attackKey;
    if (task->spawnArg1.value & GRENADE_SHELL_COMPANION_SHOT) {
        work->sphereBody.key = attackKey | GRENADE_SHELL_ATTACK_COMPANION_BIT;
    }
    work->sphereBody.radius = GRENADE_SHELL_FLIGHT_SPHERE_RADIUS;
    work->sphereBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->sphereBody);
    worldCollisionInitContacts(work->sphereBody.context.contacts, ARRAY_SIZE(work->sphereContacts), 0);
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->capsule.contacts            = work->capsuleContacts;
    work->capsuleBody.coord           = rootCoord;
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
    work->capsule.end0Radius          = GRENADE_SHELL_CAPSULE_END_RADIUS;
    work->capsule.end1Radius          = GRENADE_SHELL_CAPSULE_END_RADIUS;
    work->sphereBody.flags           |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->capsule.ends[1].vz          = -(work->flightTimer.word >> GRENADE_SHELL_INITIAL_CAPSULE_SHIFT);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->capsuleBody);
    worldCollisionInitContacts(work->capsule.contacts, ARRAY_SIZE(work->capsuleContacts), 0);
    work->capsuleBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
}

/// Places a grenade at its launcher's muzzle and arms its flight collision bodies.
///
/// Requires a live projectile model whose root is parented to its muzzle.
/// `spawnArg1.value` packs the loaded round in bits 0..7, weapon index in
/// 8..15, launcher row (0 Grenade Pistol, 1 MM1) in 16..19 and companion-shot
/// flag in bit 20. The row selects the muzzle offset and initial Q16.16 flight
/// divisor, independently of ammunition. Both tables must contain that row.
///
/// Allocates task-owned work and installs its exit callback; allocation failure
/// releases scratch and kills the task. Success advances to flight, reparents
/// the model to the world coordinate, records a Q12 unit direction and links
/// one attack sphere plus a grid-probing capsule. Teardown unlinks both bodies;
/// ordinary task teardown frees work. Borrows one SVECTOR only during the call.
static void _grenadeShellSpawn(Task* task)
{
    enum {
        GRENADE_SHELL_LAUNCHER_ROW_SHIFT = 16,
        GRENADE_SHELL_LAUNCHER_ROW_MASK  = 15
    };

    SVECTOR*           scratchHead;
    SVECTOR*           muzzleOffset;
    SVECTOR*           gteMuzzleOffset;
    MATRIX*            rootMatrix;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          muzzleCoord;
    WeaponGrenadeWork* work;
    s32                launcherRow;
    s32                initialFlightDivisor;

    scratchHead                   = SCRATCH_STACK_CURSOR(SVECTOR);
    muzzleOffset                  = scratchHead - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = muzzleOffset;
    model                         = task->extra.tmd;
    launcherRow                   = ((u32)task->spawnArg1.value >> GRENADE_SHELL_LAUNCHER_ROW_SHIFT) & GRENADE_SHELL_LAUNCHER_ROW_MASK;
    rootCoord                     = model->coords;
    muzzleCoord                   = rootCoord->parent;
    work                          = memCalloc(sizeof(WeaponGrenadeWork), 0);
    gteMuzzleOffset               = muzzleOffset;
    if (work == NULL) {
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
        taskKill(task);
        return;
    }
    task->work         = work;
    task->exitCallback = _grenadeShellExit;
    task->state++;
    memFillBytes(work, 0, sizeof(*work));

    // Place the muzzle offset through its cached pose, then remove the view transform.
    muzzleOffset->vx          = gGrenadeShellMuzzleOffsets[launcherRow].vx;
    muzzleOffset->vy          = gGrenadeShellMuzzleOffsets[launcherRow].vy;
    muzzleOffset->vz          = gGrenadeShellMuzzleOffsets[launcherRow].vz;
    muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(muzzleCoord);
    rootCoord->workm = muzzleCoord->workm;
    gte_SetRotMatrix(&muzzleCoord->workm);
    gte_SetTransMatrix(&muzzleCoord->workm);
    gte_ldv0(gteMuzzleOffset);
    gte_rtv0tr();
    gte_stlvnl(rootCoord->workm.t);
    rootMatrix = &rootCoord->coord;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &rootCoord->workm, rootMatrix);
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags            = 0;
    gfxRotMatrixX(rootMatrix, -(ACTOR_TRANSFORM_ANGLE_TURN / 4), GRAPHICS_ROTATION_COMPOSE);
    gfxReadMatrixZAxis(rootMatrix, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    // The flight divisor grows each frame, reducing the direction-based step.
    initialFlightDivisor = gGrenadeShellSpeeds[launcherRow];
    _grenadeShellLinkFlightBodies(task, work, rootCoord, initialFlightDivisor);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
