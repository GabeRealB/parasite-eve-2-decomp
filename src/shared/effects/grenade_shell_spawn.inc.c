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
    muzzleOffset->vx          = _gGrenadeShellMuzzleOffsets[launcherRow].vx;
    muzzleOffset->vy          = _gGrenadeShellMuzzleOffsets[launcherRow].vy;
    muzzleOffset->vz          = _gGrenadeShellMuzzleOffsets[launcherRow].vz;
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
    initialFlightDivisor = _gGrenadeShellInitialFlightDivisors[launcherRow];
    _grenadeShellLinkFlightBodies(task, work, rootCoord, initialFlightDivisor);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#include "gameplay/world_coords.h"

/* Part of the grenade shell library; see grenade_shell.h. */

/// Scratch-stack block the shell's flight state holds for one frame.
///
/// The block is reserved on entry and released on every way out, so nothing
/// in it survives between frames. `delta` serves two purposes in turn: it
/// first receives the push-back of the grid contact being classified, which
/// is discarded because only the surface mask returned beside it is used, and
/// then holds the frame's step, in whole coordinate units, that is added onto
/// the projectile's coordinate. The last two words are filled only on
/// detonation, from the shot packed in `Task::spawnArg1`.
typedef struct {
    byte                field_0[0x20];   // No recovered access; role unproven
    WorldCollisionDelta delta;           // Contact push-back output, then this frame's translation
    s32                 weaponIndexBits; // Firing weapon's index, still at bits 8..15 as the spawn argument packs it
    s32                 ammunitionIndex; // Loaded round on detonation (GRENADE_ROUND_*: 0xA fragmentation, 0xB airburst, 0xC riot)
} _GrenadeShellFlightScratch;
STATIC_ASSERT_SIZEOF(_GrenadeShellFlightScratch, 0x38);

/// Integrates Q12 direction by the positive integer half of the flight divisor.
///
/// The signed integer half must be nonzero and positive. Division truncates
/// toward zero, stores three whole game-coordinate steps and adds them to the
/// root's parent-frame translation. Requires live work/root and a reserved
/// flight scratch block; the caller owns its release and cache invalidation.
static inline void _grenadeShellIntegrateFlight(WeaponGrenadeWork* work, GfxCoord* coord, _GrenadeShellFlightScratch* scratch)
{
    scratch->delta.vector.vx = work->dir.vx / work->flightTimer.halves.integer;
    scratch->delta.vector.vy = work->dir.vy / work->flightTimer.halves.integer;
    scratch->delta.vector.vz = work->dir.vz / work->flightTimer.halves.integer;
    coord->coord.t[0]       += scratch->delta.vector.vx;
    coord->coord.t[1]       += scratch->delta.vector.vy;
    coord->coord.t[2]       += scratch->delta.vector.vz;
}

/// Advances a launcher grenade, or changes its attack sphere into a timed blast.
///
/// Requires a spawned model/work, initialized collision lists and a positive
/// Q16.16 flight divisor. Enemy-body contact detonates independently of grids.
/// A grid surface detonates when it blocks probes and accepts weapon impacts;
/// a blocking surface that ignores impacts selects teardown but still moves
/// this tick. Surface class 1 in saved area 20 of Dryfield day/night detonates
/// only after the probe-pass test. Timeout also detonates after translation.
/// Motion divides the Q12 direction by the divisor's integer half, grows the
/// divisor and applies positive-Y gravity; smoke thins from every tick to four.
///
/// `spawnArg1.value` packs round 10..12 in bits 0..7, weapon index in 8..15,
/// launcher row in 16..19 and companion-shot flag at bit 20. Detonation uses
/// the round's blast radius, explosion effect and weapon sound (fixed companion
/// sound for bit 20), then keeps the sphere live for 8 ticks, or 1 for airburst.
/// Scratch is borrowed for this call and released on every exit; collision
/// bodies remain linked until task teardown. No input pointer is retained.
static void _grenadeShellFly(Task* task)
{
    enum {
        GRENADE_SHELL_SHOT_WEAPON_INDEX_MASK     = 0xFF00,
        GRENADE_SHELL_SHOT_COMPANION             = 1 << 20,
        GRENADE_SHELL_STATE_BLAST                = 2,
        GRENADE_SHELL_STATE_EXIT                 = 3,
        GRENADE_SHELL_BLAST_FRAMES               = 8,
        GRENADE_SHELL_AIRBURST_FRAMES            = 1,
        GRENADE_SHELL_FLIGHT_TIMEOUT             = 0xDFFFF,
        GRENADE_SHELL_GRAVITY_STEP               = 16,
        GRENADE_SHELL_CAPSULE_REACH_SHIFT        = 9,
        GRENADE_SHELL_SMOKE_MAX_INTERVAL         = 4,
        GRENADE_SHELL_SMOKE_INTERVAL_STEP_FRAMES = 7,
        GRENADE_SHELL_SCRIPTED_SURFACE_CLASS     = 1,
        GRENADE_SHELL_SCRIPTED_SAVED_AREA        = GAME_AREA_DRYFIELD_WATER_TOWER,
        GRENADE_SHELL_SOUND_COMPANION            = 0x40660002,
        GRENADE_SHELL_WEAPON_SOUND_INDEX_SHIFT   = 8,
        GRENADE_SHELL_ROUND_SOUND_VARIANT_SHIFT  = 24,
        GRENADE_SHELL_SCRIPTED_STAGE_COUNT       = GAME_STAGE_DRYFIELD_NIGHT - GAME_STAGE_DRYFIELD + 1,
        GRENADE_SHELL_SOUND_ROUND_BASE           = 0x20000005
    };
    _GrenadeShellFlightScratch*      scratchHead;
    _GrenadeShellFlightScratch*      scratch;
    WeaponGrenadeWork*               work;
    GfxCoord*                        coord;
    WorldCollisionSurfaceProperties* surface;
    s32                              surfaceClass;
    s32                              blastFrames;
    s32                              flightFrame;
    s32                              roundSoundBits;
    s32                              weaponSoundBits;

    work        = task->work;
    coord       = task->extra.tmd->coords;
    scratchHead = SCRATCH_STACK_CURSOR(_GrenadeShellFlightScratch);
    // Keep both addresses: classification uses the pre-reservation head,
    // while movement uses the reserved block. The two contact paths join below.
    SCRATCH_STACK_CURSOR(_GrenadeShellFlightScratch) = scratchHead - 1;
    scratch                                          = scratchHead - 1;
    coord->composeStamp                              = GRAPHICS_COORD_DIRTY;
    if (worldCollisionCountContactsByKind(work->sphereContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
    explode:
        // The shot arrives packed in the spawn argument: the loaded round in
        // the low byte and the firing weapon's index in the byte above it.
        scratch->weaponIndexBits = task->spawnArg1.value & GRENADE_SHELL_SHOT_WEAPON_INDEX_MASK;
        scratch->ammunitionIndex = (u8)task->spawnArg1.value;
        task->state              = GRENADE_SHELL_STATE_BLAST;
        effectSpawn(EFFECT_GRENADE_EXPLOSION, coord, scratch->ammunitionIndex, NULL);
        // Keep each sound path's call; their shared tail merges in the image.
        if (task->spawnArg1.value & GRENADE_SHELL_SHOT_COMPANION) {
            worldCoordPlaySound(coord, GRENADE_SHELL_SOUND_COMPANION, 1);
        } else {
            weaponSoundBits = scratch->weaponIndexBits << GRENADE_SHELL_WEAPON_SOUND_INDEX_SHIFT;
            roundSoundBits  = ((scratch->ammunitionIndex - GRENADE_ROUND_FIRST) << GRENADE_SHELL_ROUND_SOUND_VARIANT_SHIFT) | GRENADE_SHELL_SOUND_ROUND_BASE;
            worldCoordPlaySound(coord, weaponSoundBits | roundSoundBits, 1);
        }
        blastFrames = GRENADE_SHELL_BLAST_FRAMES;
        if (scratch->ammunitionIndex == GRENADE_ROUND_AIRBURST) {
            blastFrames = GRENADE_SHELL_AIRBURST_FRAMES;
        }
        work->flightTimer.word  = blastFrames;
        work->sphereBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        // The round is read back once more after the block's release; nothing
        // reserves scratch in between.
        SCRATCH_STACK_RELEASE_BLOCK(_GrenadeShellFlightScratch);
        work->sphereBody.radius = _gGrenadeShellBlastRadii[scratch->ammunitionIndex - GRENADE_ROUND_FIRST];
        return;
    }

    // Classify the capsule first, then the sphere. The join after each mask
    // assignment retains the original class reload and contact-call ordering.
    if (worldCollisionCountContactsByKind(work->capsuleContacts, WORLD_COLLISION_CONTACT_GRID) == 0) {
        goto trySphereContacts;
    }
    worldCollisionResolveResponsePushback(work->capsuleContacts, &(scratchHead - 1)->delta, ARRAY_SIZE(work->capsuleContacts), &surfaceClass);
    surfaceClass = worldCollisionSurfaceClassFromMask((const u8*)&surfaceClass);
classified:
    surface = gWorldCollisionSurfacePropertiesTables[gGameSession->location.stage - 1][gGameSession->location.area - 1][surfaceClass];
    if (surface->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
        if (surface->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
            goto explode;
        }
        task->state = GRENADE_SHELL_STATE_EXIT;
        goto move;
    }
    if (surfaceClass == GRENADE_SHELL_SCRIPTED_SURFACE_CLASS && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area == GRENADE_SHELL_SCRIPTED_SAVED_AREA && (u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage - GAME_STAGE_DRYFIELD) < (u32)GRENADE_SHELL_SCRIPTED_STAGE_COUNT) {
        goto explode;
    }
    goto move;
trySphereContacts:
    if (worldCollisionCountContactsByKind(work->sphereContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        worldCollisionResolveResponsePushback(work->sphereContacts, &(scratchHead - 1)->delta, ARRAY_SIZE(work->sphereContacts), &surfaceClass);
        surfaceClass = worldCollisionSurfaceClassFromMask((const u8*)&surfaceClass);
        goto classified;
    }
move:
    _grenadeShellIntegrateFlight(work, coord, scratch);
    work->capsule.ends[1].vz = -(work->flightTimer.word >> GRENADE_SHELL_CAPSULE_REACH_SHIFT);
    work->flightTimer.word  += GRENADE_SHELL_FLIGHT_DIVISOR_STEP;
    if (work->flightTimer.word > GRENADE_SHELL_FLIGHT_TIMEOUT) {
        goto explode;
    }
    work->dir.vy      = work->dir.vy + GRENADE_SHELL_GRAVITY_STEP;
    flightFrame       = work->flightFrame + 1;
    work->flightFrame = flightFrame;
    if (work->smokeInterval < GRENADE_SHELL_SMOKE_MAX_INTERVAL && flightFrame % GRENADE_SHELL_SMOKE_INTERVAL_STEP_FRAMES == 0) {
        work->smokeInterval = work->smokeInterval + 1;
    }
    if (work->flightFrame % work->smokeInterval == 0) {
        effectSpawn(EFFECT_SMOKE_PUFF, coord, 0, NULL);
    }
    worldCollisionClearContacts(work->sphereContacts);
    worldCollisionClearContacts(work->capsuleContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_GrenadeShellFlightScratch);
}
