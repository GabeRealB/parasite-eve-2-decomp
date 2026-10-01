/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame state of the effect child set up by `golemPawnRookBulletSpawn`, entry
/// 1 of `Actor05600_D0008C`. `gSceneCombatState.actorControl` overrides it: 0 shows the child and
/// runs the tick, 1 only refreshes its colour, 2 hides it. The tick moves the
/// child along its own Y axis, spawns a puff every fourth frame and ends the
/// flight on a body contact, a surface that blocks probes, or after
/// 0x5A frames: the burst effect and cue play, the child hides and advances
/// to state 2, and a kind-1 body contact also starts a pad rumble.
void golemPawnRookBulletFly(Enemy* arg0, Task* arg1)
{
    GolemPawnRookFxWork* work;
    GfxCoord*            coord;
    TmdObject*           tmd;
    SVECTOR*             scratch;
    Enemy*               ctx;
    s32                  found;
    s32                  idx;
    s32                  sound;
    s32                  pan;
    VECTOR               pos;

    tmd   = arg1->extra.tmd;
    coord = tmd->coords;
    work  = (GolemPawnRookFxWork*)arg1->work;
    found = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
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
    if (++work->field_E8 >= 4) {
        scratch->vx = 0;
        scratch->vy = 0x64;
        scratch->vz = 0;
        Gp_SpawnEff(0x60070, coord, 0x01001600, scratch);
        work->field_E8 = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);

    if (work->recD0[0].key.value != 0) {
        idx = func_800E1B24(work->recD0[0].key.value);
        if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx]->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            found = 1;
        }
        Gp_ClearRec18Occupied(work->recD0);
    }
    if (work->rec60[0].key.value != 0 || found || ++work->field_EA >= 0x5A) {
        Gp_SpawnEff(D_80115750, coord, (s32)(work->field_EE), NULL);
        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        ctx                    = arg1->spawnArg2.pointer;
        sound                  = GOLEM_PAWN_ROOK_IMPACT_SOUND | (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan                    = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        arg1->state = 2;
        if ((work->rec60[0].key.value & 0xFFFF0080) == 0x10000) {
            Gp_SpawnPadLerp(0xA, 0xFF, 8);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}
