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

/// Flight state of the projectile. Detonates when `sphereContacts` holds a
/// category-3 contact (packed kind 0x30000), when a blocking surface accepts
/// weapon impacts, or when `flightTimer` passes 0xDFFFF. Surface index 1 in
/// area 0x14 of stages 2 and 3 also detonates. Otherwise it steps the
/// projectile by `dir / flightTimer.halves.integer`, lets gravity pull
/// `dir.vy` down, and trails smoke every `smokeInterval` flight frames. That
/// period starts at 1 and grows by one every seven `flightFrame`s, up to four,
/// so the trail thins as the grenade slows.
///
/// The shot is fired through `Task::spawnArg1`: its low byte is the
/// attachment id driving the explosion effect, the byte above it seeds the
/// sound bank, and bit 0x100000 marks the shot that plays the fixed
/// `0x40660002` clip instead.
void grenadeShellFly(Task* arg0)
{
    _GrenadeShellFlightScratch*      head;
    _GrenadeShellFlightScratch*      scratch;
    WeaponGrenadeWork*               work;
    GfxCoord*                        coord;
    WorldCollisionSurfaceProperties* surface;
    s32                              idx;
    s32                              clip;
    s32                              step;
    s32                              sfxarg;
    s32                              sfxbase;

    work  = (WeaponGrenadeWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    head  = SCRATCH_STACK_CURSOR(_GrenadeShellFlightScratch);
    /* Pushed and then re-derived rather than stored from `scratch`: the scratch
       head has to stay live in its own register, because the `WorldCollisionDelta`
       handed to `worldCollisionResolveResponsePushback` below is addressed off it and not off `scratch`. */
    SCRATCH_STACK_CURSOR(_GrenadeShellFlightScratch) = head - 1;
    scratch                                          = head - 1;
    coord->composeStamp                              = GRAPHICS_COORD_DIRTY;
    if (worldCollisionCountContactsByKind(work->sphereContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
    explode:
        // The shot arrives packed in the spawn argument: the loaded round in
        // the low byte and the firing weapon's index in the byte above it.
        scratch->weaponIndexBits = arg0->spawnArg1.value & 0xFF00;
        scratch->ammunitionIndex = (u8)arg0->spawnArg1.value;
        arg0->state              = 2;
        Gp_SpawnEff(EFFECT_GRENADE_EXPLOSION, coord, scratch->ammunitionIndex, NULL);
        /* Two calls, not one call on a selected argument: the identical tails
           are what cross-jumping merges into a single `jal` with an unfilled
           delay slot. */
        if (arg0->spawnArg1.value & 0x100000) {
            Gp_PlayObjSfx(coord, 0x40660002, 1);
        } else {
            sfxbase = scratch->weaponIndexBits << 8;
            sfxarg  = ((scratch->ammunitionIndex - GRENADE_ROUND_FIRST) << 24) | 0x20000005;
            Gp_PlayObjSfx(coord, sfxbase | sfxarg, 1);
        }
        clip = 8;
        if (scratch->ammunitionIndex == GRENADE_ROUND_AIRBURST) {
            clip = 1;
        }
        work->flightTimer.word  = clip;
        work->sphereBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        // The round is read back once more after the block's release; nothing
        // reserves scratch in between.
        SCRATCH_STACK_RELEASE_BLOCK(_GrenadeShellFlightScratch);
        work->sphereBody.radius = gGrenadeShellBlastRadii[scratch->ammunitionIndex - GRENADE_ROUND_FIRST];
        return;
    }

    /* Each contact list classifies its own contact: the two call pairs are
       written out, and cross-jumping merges them into the one the image has.
       The list that is tried second sits after the surface tests and joins
       them at `classified`, which is why the class is read back from `idx`'s
       stack slot there. Both pairs address the delta off `head`, not
       `scratch`. */
    if (worldCollisionCountContactsByKind(work->capsuleContacts, WORLD_COLLISION_CONTACT_GRID) == 0) {
        goto trySphereContacts;
    }
    worldCollisionResolveResponsePushback(work->capsuleContacts, &(head - 1)->delta, 1, &idx);
    idx = worldCollisionSurfaceClassFromMask((const u8*)&idx);
classified:
    surface = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx];
    if (surface->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
        if (surface->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
            goto explode;
        }
        arg0->state = 3;
        goto move;
    }
    if (idx == 1 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area == 0x14 && (u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage - 2) < 2U) {
        goto explode;
    }
    goto move;
trySphereContacts:
    if (worldCollisionCountContactsByKind(work->sphereContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        worldCollisionResolveResponsePushback(work->sphereContacts, &(head - 1)->delta, 1, &idx);
        idx = worldCollisionSurfaceClassFromMask((const u8*)&idx);
        goto classified;
    }
move:
    scratch->delta.vector.vx = work->dir.vx / work->flightTimer.halves.integer;
    scratch->delta.vector.vy = work->dir.vy / work->flightTimer.halves.integer;
    scratch->delta.vector.vz = work->dir.vz / work->flightTimer.halves.integer;
    coord->coord.t[0]       += scratch->delta.vector.vx;
    coord->coord.t[1]       += scratch->delta.vector.vy;
    coord->coord.t[2]       += scratch->delta.vector.vz;
    work->capsule.ends[1].vz = -(work->flightTimer.word >> 9);
    work->flightTimer.word  += GRENADE_SHELL_FLIGHT_STEP;
    if (work->flightTimer.word > 0xDFFFF) {
        goto explode;
    }
    work->dir.vy      = work->dir.vy + 0x10;
    step              = work->flightFrame + 1;
    work->flightFrame = step;
    if (work->smokeInterval < 4 && step % 7 == 0) {
        work->smokeInterval = work->smokeInterval + 1;
    }
    if (work->flightFrame % work->smokeInterval == 0) {
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0, NULL);
    }
    worldCollisionClearContacts(work->sphereContacts);
    worldCollisionClearContacts(work->capsuleContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_GrenadeShellFlightScratch);
}
