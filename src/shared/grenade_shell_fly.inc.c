/* Part of the grenade shell library; see grenade_shell.h. */

/// Flight state of the projectile. Detonates when the shot
/// has touched world geometry (`rec0` with 0x30000), when a blocking surface
/// enables weapon impacts, or when the flight timer runs past 0xDFFFF; otherwise it steps
/// the projectile by `dir / field_88.halves.integer`, lets gravity pull `dir.vy` down,
/// and trails smoke every `field_8C` frames — a divisor that grows by one
/// every seven frames up to four, so the trail thins as the grenade slows.
///
/// The shot is fired through `Task::spawnArg1`: its low byte is the
/// attachment id driving the explosion effect, the byte above it seeds the
/// sound bank, and bit 0x100000 marks the shot that plays the fixed
/// `0x40660002` clip instead.
void grenadeShellFly(Task* arg0)
{
    WeaponGrenadeScratch*            blk;
    WeaponGrenadeWork*               work;
    GfxCoord*                        coord;
    WorldCollisionContact*           rec;
    WorldCollisionSurfaceProperties* surface;
    u8*                              head;
    s32                              idx;
    s32                              count;
    s32                              clip;
    s32                              step;
    s32                              sfxarg;
    s32                              sfxbase;

    work  = (WeaponGrenadeWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    head  = SCRATCH_STACK_CURSOR(u8);
    /* Pushed and then re-derived rather than stored from `blk`: the scratch
       head has to stay live in its own register, because the `GpDeltaScratch`
       handed to `func_800E0FEC` below is addressed off it and not off `blk`. */
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(WeaponGrenadeScratch);
    blk                      = (WeaponGrenadeScratch*)(head - sizeof(WeaponGrenadeScratch));
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    if (Gp_CountRec18Hi(work->rec0, 0x30000) != 0) {
    explode:
        blk->field_30 = arg0->spawnArg1.value & 0xFF00;
        blk->sfx      = (u8)arg0->spawnArg1.value;
        arg0->state   = 2;
        Gp_SpawnEff(EFFECT_GRENADE_EXPLOSION, coord, blk->sfx, NULL);
        /* Two calls, not one call on a selected argument: the identical tails
           are what cross-jumping merges into a single `jal` with an unfilled
           delay slot. */
        if (arg0->spawnArg1.value & 0x100000) {
            Gp_PlayObjSfx(coord, 0x40660002, 1);
        } else {
            sfxbase = blk->field_30 << 8;
            sfxarg  = ((blk->sfx - 0xA) << 24) | 0x20000005;
            Gp_PlayObjSfx(coord, sfxbase | sfxarg, 1);
        }
        clip = 8;
        if (blk->sfx == 0xB) {
            clip = 1;
        }
        work->field_88.word = clip;
        work->obj.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        SCRATCH_STACK_RELEASE_BYTES(sizeof(WeaponGrenadeScratch));
        work->obj.radius = gGrenadeShellBlastRadii[blk->sfx - 0xA];
        return;
    }

    /* `rec` is picked after each count, not before: assigning it first would
       make it cross the call and cost a call-saved register. */
    count = Gp_CountRec18Hi(work->rec1, 0x100000);
    rec   = work->rec1;
    if (count == 0) {
        goto try_rec0;
    }
check:
    /* `head - 0x18` is `&blk->delta`; spelling it off `head` is what keeps the
       two scratch pointers apart, and the reference count is what wins `head`
       the lower of the two call-saved registers. */
    SOFT_USE_REG2(head, head);
    func_800E0FEC(rec, (GpDeltaScratch*)(head - 0x18), 1, &idx);
    idx = func_800E1ACC((u8*)&idx);
    /* `func_800E1ACC` writes through `&idx` as well as returning it, so the
       index is re-read from the slot instead of kept in the return register. */
    SOFT_COMPILER_BARRIER();
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
try_rec0:
    count = Gp_CountRec18Hi(work->rec0, 0x100000);
    rec   = work->rec0;
    if (count != 0) {
        goto check;
    }
move:
    blk->delta.vx.word     = work->dir.vx / work->field_88.halves.integer;
    blk->delta.vy.word     = work->dir.vy / work->field_88.halves.integer;
    blk->delta.vz.word     = work->dir.vz / work->field_88.halves.integer;
    coord->coord.t[0]     += blk->delta.vx.word;
    coord->coord.t[1]     += blk->delta.vy.word;
    coord->coord.t[2]     += blk->delta.vz.word;
    work->d4rec.ends[1].vz = -(work->field_88.word >> 9);
    work->field_88.word   += 0x1800;
    if (work->field_88.word > 0xDFFFF) {
        goto explode;
    }
    work->dir.vy   = work->dir.vy + 0x10;
    step           = work->field_90 + 1;
    work->field_90 = step;
    if (work->field_8C < 4 && step % 7 == 0) {
        work->field_8C = work->field_8C + 1;
    }
    if (work->field_90 % work->field_8C == 0) {
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0, NULL);
    }
    Gp_ClearRec18Occupied(work->rec0);
    Gp_ClearRec18Occupied(work->rec1);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(WeaponGrenadeScratch));
}
