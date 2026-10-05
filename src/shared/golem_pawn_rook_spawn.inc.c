/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Spawn handler of the approach cycle: allocates the `GolemPawnRookWork` block,
/// binds the animation set and reseeds the nineteen slots, then starts the
/// companion enemy whose model takes its texture page and CLUT row from the
/// current room's area record. `Enemy::spawnState` picks how much of that is
/// kept: 0 also links the list node, the five `worldCollisionLinkBody` collision nodes with
/// their `WorldCollisionContact` tables and the room's streaming cue, while 1 and 2 only
/// prime the animation state. Entry 0 of `Actor05600_D00098`.
void golemPawnRookSpawn(Enemy* ctx, Task* actor)
{
    GolemPawnRookWork* work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          parts;
    GfxCoord*          partsA;
    GfxCoord*          partsB;
    GfxCoord*          partsC;
    GfxCoord*          partsD;
    GfxCoord*          effParts;
    Enemy*             eff;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                param;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    u32 lcg;
#endif

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(GolemPawnRookWork), 0);
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work                   = work;
    obj->flags                    = 0;
    coord->composeStamp           = GRAPHICS_COORD_DIRTY;
    obj->lightMtx                 = &work->lightMtx;
    obj->colorMtx                 = &work->colorMtx;
    work->actorId                 = GOLEM_PAWN_ROOK_ID;
    work->taskTable               = gGolemPawnRookTasks;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, gGolemPawnRookAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    actorTintModel(Gp_SpawnEnemyFromTable(gGolemPawnRookTasks, 3, 0, ctx)->task->extra.tmd, ctx);
#endif
    eff = Gp_SpawnEnemyFromTable(gGolemPawnRookTasks, 1, 0, ctx);
    actorTintModel(eff->task->extra.tmd, ctx);

    switch (ctx->spawnState) {
        case 0:
            ctx->field_4  = &coord->coord;
            ctx->field_48 = 0;
            Gp_LinkNode(&ctx->node);
            parts           = actor->extra.tmd->coords;
            ctx->bodyPos.vx = 0;
            ctx->bodyPos.vy = 0;
            ctx->bodyPos.vz = 0;
            ctx->param      = gGolemPawnRookParams;
            ctx->recs       = work->hurtContacts;
            ctx->coord      = &parts[3];
            ctx->hp         = gGolemPawnRookParams->hpMax;
            Gp_IncStateF0Ref(0);
            work->patrols = ctx->place->mode & 1;
            if (work->patrols == 0) {
                work->anim     = 1;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = 2;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                param                    = ctx->place->variant;
                work->patrolDistanceLeft = param * 1000;
            }

            tbl = gGolemPawnRookAreaParams[gGameSession->location.loc.stage];
            if (tbl != NULL) {
                work->soundSet = tbl[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                param1[3] = 0;
                param1[2] = 0xA;
                param1[0] = work->soundSet;
                param2[0] = GOLEM_PAWN_ROOK_ID;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            }

#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
            work->shieldHp = 0xFA;
#endif
            work->sightCapsule.ends[0].vz = 0x1F40;
            work->sightCapsule.end0Radius = 0x3E8;
            work->sightCapsule.ends[0].vx = 0;
            work->sightCapsule.ends[0].vy = 0;
            work->sightCapsule.ends[1].vx = 0;
            work->sightCapsule.ends[1].vy = 0;
            work->sightCapsule.ends[1].vz = 0;
            work->sightCapsule.end1Radius = 0x5DC;
            work->sightCapsule.contacts   = work->sightContacts;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
            lcg                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->screamCharges = ((lcg >> 16) & 1) + 1;
            gRandomLcgState     = lcg;
#endif
            partsA                          = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &partsA[4];
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody);
            worldCollisionInitContacts(work->sightContacts, ARRAY_SIZE(work->sightContacts), 0);
            work->sightBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            partsB                          = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = 0x30000 | GOLEM_PAWN_ROOK_ID;
            work->hurtBody.radius           = 0x190;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &partsB[3];
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody);
            worldCollisionInitContacts(work->hurtContacts, ARRAY_SIZE(work->hurtContacts), 0);
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

            partsC                            = actor->extra.tmd->coords;
            work->groundBody.pos.vy           = -0x226;
            work->groundBody.context.contacts = work->groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0;
            work->groundBody.radius           = 0x226;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->groundBody.coord            = partsC;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody);
            worldCollisionInitContacts(work->groundContacts, ARRAY_SIZE(work->groundContacts), 0);
            work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            effParts                          = eff->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = 0x1F4;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = 0x1F4;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = effParts;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody);
            worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

            work->laserCapsule.ends[0].vx   = 0;
            work->laserCapsule.ends[0].vy   = 0;
            work->laserCapsule.ends[0].vz   = 0;
            work->laserCapsule.ends[1].vx   = 0;
            work->laserCapsule.ends[1].vy   = 0;
            work->laserCapsule.ends[1].vz   = 0;
            work->laserCapsule.end0Radius   = 1;
            work->laserCapsule.end1Radius   = 1;
            work->laserCapsule.contacts     = work->laserContacts;
            partsD                          = actor->extra.tmd->coords;
            work->laserBody.context.capsule = &work->laserCapsule;
            work->laserBody.pos.vx          = 0;
            work->laserBody.pos.vy          = 0;
            work->laserBody.pos.vz          = 0;
            work->laserBody.key             = 0;
            work->laserBody.radius          = 0;
            work->laserBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->laserBody.coord           = partsD;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->laserBody);
            worldCollisionInitContacts(work->laserContacts, ARRAY_SIZE(work->laserContacts), 0);
            work->laserBody.flags = (work->laserBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            actor->state          = 1;
            break;
        case 1:
            work->anim   = 0x19;
            work->step   = 2;
            actor->state = 2;
            break;
        case 2:
            work->anim   = 0x1D;
            work->step   = 2;
            actor->state = 2;
            break;
    }
}
