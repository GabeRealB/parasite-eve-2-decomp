/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Advances an independently flying launcher grenade and bursts it on impact.
///
/// grenade owns live grenade work and linked collision bodies. The Q12 local
/// Y axis advances it 150 game units per running frame using 75 >> 11 scaling.
/// Paused frames only sample colour; hidden frames skip drawing. A strike,
/// probe-blocking wall, or 90 running frames starts hidden teardown and the room
/// burst effect; a player-body strike also rumbles. The unused enemy parameter
/// preserves the lifecycle callback signature. Flight scratch is released here.
static void _golemPawnRookGrenadeFly(Enemy* unusedEnemy, Task* grenade)
{
    /// Flight's 40-byte reservation; only the puff's local offset is identified.
    typedef struct {
        SVECTOR puffOffset;       // smoke spawn offset along the grenade's local axes
        u8      unknown_08[0x20]; // reserved bytes with no recovered access; role and layout unproven
    } _GolemPawnRookGrenadeFlightScratch;
    STATIC_ASSERT_SIZEOF(_GolemPawnRookGrenadeFlightScratch, 0x28);
    enum {
        GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR       = 75,
        GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT           = 11,
        GOLEM_PAWN_ROOK_GRENADE_PUFF_FRAMES          = 4,
        GOLEM_PAWN_ROOK_GRENADE_PUFF_OFFSET_Y        = 100,
        GOLEM_PAWN_ROOK_GRENADE_SMOKE_ARGUMENT       = 0x01001600, // size 1536, period 1, drifting mode 1
        GOLEM_PAWN_ROOK_GRENADE_FLIGHT_FRAMES        = 90,
        GOLEM_PAWN_ROOK_GRENADE_COMPANION_BIT        = 0x80,
        GOLEM_PAWN_ROOK_GRENADE_RUMBLE_FRAMES        = 10,
        GOLEM_PAWN_ROOK_GRENADE_RUMBLE_POWER         = 255,
        GOLEM_PAWN_ROOK_GRENADE_RUMBLE_END_INTENSITY = 8,
    };

    GolemPawnRookGrenadeWork*           work;
    GfxCoord*                           root;
    TmdObject*                          model;
    _GolemPawnRookGrenadeFlightScratch* flightScratch;
    Enemy*                              spawnEnemy;
    s32                                 blockedSurface;
    s32                                 surfaceClass;
    s32                                 soundId;
    s32                                 audioPan;
    VECTOR                              worldPos;

    model          = grenade->extra.tmd;
    root           = model->coords;
    work           = grenade->work;
    blockedSurface = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldPos.vx = root->workm.t[0];
            worldPos.vy = root->workm.t[1];
            worldPos.vz = root->workm.t[2];
            worldCoordUpdateActorColor(grenade->spawnArg2.pointer, &worldPos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    root->composeStamp = GRAPHICS_COORD_DIRTY;
    root->coord.t[0]  += (root->coord.m[0][1] * GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR) >> GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT;
    root->coord.t[1]  += (root->coord.m[1][1] * GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR) >> GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT;
    root->coord.t[2]  += (root->coord.m[2][1] * GOLEM_PAWN_ROOK_GRENADE_MOVE_NUMERATOR) >> GOLEM_PAWN_ROOK_GRENADE_MOVE_SHIFT;

    flightScratch = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookGrenadeFlightScratch);
    if (++work->timer >= GOLEM_PAWN_ROOK_GRENADE_PUFF_FRAMES) {
        flightScratch->puffOffset.vx = 0;
        flightScratch->puffOffset.vy = GOLEM_PAWN_ROOK_GRENADE_PUFF_OFFSET_Y;
        flightScratch->puffOffset.vz = 0;
        effectSpawn(EFFECT_SMOKE_PUFF, root, GOLEM_PAWN_ROOK_GRENADE_SMOKE_ARGUMENT, &flightScratch->puffOffset);
        work->timer = 0;
    }
    worldPos.vx = root->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(grenade->spawnArg2.pointer, &worldPos, 0, 0);

    if (work->wallContacts[0].key.value != 0) {
        surfaceClass = worldCollisionSurfaceClassFromKey(work->wallContacts[0].key.value);
        if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][surfaceClass]->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            blockedSurface = 1;
        }
        worldCollisionClearContacts(work->wallContacts);
    }
    if (work->strikeContacts[0].key.value != 0 || blockedSurface || ++work->flightFrames >= GOLEM_PAWN_ROOK_GRENADE_FLIGHT_FRAMES) {
        effectSpawn(gRoomEffectSparkBurstId, root, (s32)work->burstStyle, NULL);
        grenade->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        spawnEnemy                = grenade->spawnArg2.pointer;
        soundId                   = gGolemPawnRookImpactSound | (((u16)spawnEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
        audioPan                  = (s8)worldCoordGetOriginAudioPan(root);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
        grenade->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
        if ((work->strikeContacts[0].key.value & (WORLD_COLLISION_CONTACT_KIND_MASK | GOLEM_PAWN_ROOK_GRENADE_COMPANION_BIT)) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            padScriptSpawnVariableMotorRamp(GOLEM_PAWN_ROOK_GRENADE_RUMBLE_FRAMES, GOLEM_PAWN_ROOK_GRENADE_RUMBLE_POWER, GOLEM_PAWN_ROOK_GRENADE_RUMBLE_END_INTENSITY);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookGrenadeFlightScratch);
}
