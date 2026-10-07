/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame state of the effect child set up by `_golemPawnRookGrenadeSpawn`, entry
/// 1 of `Actor05600_D0008C`. `gSceneCombatState.actorControl` overrides it: 0 shows the child and
/// runs the tick, 1 only refreshes its colour, 2 hides it. The tick moves the
/// child along its own Y axis, spawns a puff every fourth frame and ends the
/// flight on a body contact, a surface that blocks probes, or after
/// 0x5A frames: the burst effect and cue play, the child hides and advances
/// to state 2, and a kind-1 body contact also starts a pad rumble.
void golemPawnRookBulletFly(Enemy* arg0, Task* arg1)
{
    GolemPawnRookGrenadeWork* work;
    GfxCoord*                 coord;
    TmdObject*                tmd;
    SVECTOR*                  scratch;
    Enemy*                    ctx;
    s32                       found;
    s32                       idx;
    s32                       sound;
    s32                       pan;
    VECTOR                    pos;

    tmd   = arg1->extra.tmd;
    coord = tmd->coords;
    work  = arg1->work;
    found = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]  += (coord->coord.m[0][1] * 75) >> 11;
    coord->coord.t[1]  += (coord->coord.m[1][1] * 75) >> 11;
    coord->coord.t[2]  += (coord->coord.m[2][1] * 75) >> 11;

    scratch = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(0x28);
    if (++work->timer >= 4) {
        scratch->vx = 0;
        scratch->vy = 0x64;
        scratch->vz = 0;
        effectSpawn(EFFECT_SMOKE_PUFF, coord, 0x01001600, scratch);
        work->timer = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);

    if (work->wallContacts[0].key.value != 0) {
        idx = worldCollisionSurfaceClassFromKey(work->wallContacts[0].key.value);
        if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx]->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            found = 1;
        }
        worldCollisionClearContacts(work->wallContacts);
    }
    if (work->strikeContacts[0].key.value != 0 || found || ++work->flightFrames >= 0x5A) {
        effectSpawn(gRoomEffectSparkBurstId, coord, (s32)(work->burstStyle), NULL);
        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        ctx                    = arg1->spawnArg2.pointer;
        sound                  = gGolemPawnRookImpactSound | (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan                    = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        arg1->state = 2;
        if ((work->strikeContacts[0].key.value & 0xFFFF0080) == 0x10000) {
            Gp_SpawnPadLerp(0xA, 0xFF, 8);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}
