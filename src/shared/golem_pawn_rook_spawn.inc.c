/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Initializes a launcher-equipped GOLEM body, its weapon and optional shield.
///
/// enemy is the body's placement record and actor its nineteen-part TMD task.
/// Allocates body-owned work and lighting, initializes animation slots 1..18,
/// and spawns the launcher (task-table entry 1) plus a Rook shield (entry 3).
/// Children inherit placement texture offsets. A fresh placement links combat
/// bodies and lock-on and requests its sound bank; saved downed poses start
/// the persistent corpse state without relinking combat. Body-work allocation
/// failure destroys the pair. The carrier must provide successful child spawns,
/// a task table containing those entries, and valid stage/area sound indices.
static void _golemPawnRookSpawn(Enemy* enemy, Task* actor)
{

    enum {
        GOLEM_PAWN_ROOK_BODY_PART                = 3,
        GOLEM_PAWN_ROOK_SIGHT_PART               = 4,
        GOLEM_PAWN_ROOK_WEAPON_TASK              = 1,
        GOLEM_PAWN_ROOK_SHIELD_TASK              = 3,
        GOLEM_PAWN_ROOK_HIT_EFFECT_SIZE          = 1280,
        GOLEM_PAWN_ROOK_HIT_EFFECT_KIND          = 2,
        GOLEM_PAWN_ROOK_PATROL_UNITS_PER_VARIANT = 1000,
        GOLEM_PAWN_ROOK_SOUND_FILE_GROUP         = 10,
        GOLEM_PAWN_ROOK_SHIELD_HP                = 250,
        GOLEM_PAWN_ROOK_SIGHT_LENGTH             = 8000,
        GOLEM_PAWN_ROOK_SIGHT_FAR_RADIUS         = 1000,
        GOLEM_PAWN_ROOK_SIGHT_NEAR_RADIUS        = 1500,
        GOLEM_PAWN_ROOK_HURT_RADIUS              = 400,
        GOLEM_PAWN_ROOK_GROUND_RADIUS            = 550,
        GOLEM_PAWN_ROOK_STRIKE_OFFSET_Y          = 500,
        GOLEM_PAWN_ROOK_STRIKE_RADIUS            = 500,
        GOLEM_PAWN_ROOK_RESTORED_CORPSE_STEP     = 2,
        GOLEM_PAWN_ROOK_CORPSE_BEHIND_ANIM       = 0x19,
        GOLEM_PAWN_ROOK_CORPSE_FRONT_ANIM        = 0x1D,
        GOLEM_PAWN_ROOK_IDLE_ANIM                = 1,
        GOLEM_PAWN_ROOK_FRESH_PLACEMENT          = 0,
        GOLEM_PAWN_ROOK_PLACEMENT_PATROLS        = 1,
    };

    GolemPawnRookWork* work;
    TmdObject*         model;
    GfxCoord*          root;
    GfxCoord*          bodyCoords;
    GfxCoord*          sightCoords;
    GfxCoord*          hurtCoords;
    GfxCoord*          groundCoords;
    GfxCoord*          laserCoords;
    GfxCoord*          weaponCoords;
    Enemy*             weaponEnemy;
    u16*               areaSoundSets;
    u8                 soundFileKey[8];
    u8                 soundFileArgs[8];
    s32                slotIndex;
    s32                patrolLengthThousands;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    u32 randomDraw;
#endif

    // The body owns the shared lighting and combat work for all attached children.
    model = actor->extra.tmd;
    root  = model->coords;
    work  = memCalloc(sizeof(GolemPawnRookWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work                   = work;
    model->flags                  = 0;
    root->composeStamp            = GRAPHICS_COORD_DIRTY;
    model->lightMtx               = &work->lightMtx;
    model->colorMtx               = &work->colorMtx;
    work->actorId                 = GOLEM_PAWN_ROOK_ID;
    work->taskTable               = gGolemPawnRookTasks;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[GOLEM_PAWN_ROOK_BODY_PART];
    work->hitEffectArg.spawnArgLo = GOLEM_PAWN_ROOK_HIT_EFFECT_SIZE;
    work->hitEffectArg.spawnArgHi = GOLEM_PAWN_ROOK_HIT_EFFECT_KIND;
    animationInitContext(&work->rig.anim, gGolemPawnRookAnimSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, GOLEM_PAWN_ROOK_IDLE_ANIM);
    }
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    _actorRenderApplyPlacementTextureOffsets(enemySpawnFromTable(gGolemPawnRookTasks, GOLEM_PAWN_ROOK_SHIELD_TASK, 0, enemy)->task->extra.tmd, enemy);
#endif
    weaponEnemy = enemySpawnFromTable(gGolemPawnRookTasks, GOLEM_PAWN_ROOK_WEAPON_TASK, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(weaponEnemy->task->extra.tmd, enemy);

    // Saved corpses keep their visual children but never rejoin combat.
    switch (enemy->spawnState) {
        case GOLEM_PAWN_ROOK_FRESH_PLACEMENT:
            enemy->field_4  = &root->coord;
            enemy->field_48 = 0;
            worldTargetLinkNode(&enemy->node);
            bodyCoords        = actor->extra.tmd->coords;
            enemy->bodyPos.vx = 0;
            enemy->bodyPos.vy = 0;
            enemy->bodyPos.vz = 0;
            enemy->param      = gGolemPawnRookParams;
            enemy->recs       = work->hurtContacts;
            enemy->coord      = &bodyCoords[GOLEM_PAWN_ROOK_BODY_PART];
            enemy->hp         = gGolemPawnRookParams->hpMax;
            sceneAcquireBattleRef(0);
            work->patrols = enemy->place->mode & GOLEM_PAWN_ROOK_PLACEMENT_PATROLS;
            if (work->patrols == 0) {
                work->anim     = GOLEM_PAWN_ROOK_IDLE_ANIM;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                patrolLengthThousands    = enemy->place->variant;
                work->patrolDistanceLeft = patrolLengthThousands * GOLEM_PAWN_ROOK_PATROL_UNITS_PER_VARIANT;
            }

            // Enqueue the placement-specific bank; only the first four bytes are read.
            areaSoundSets = gGolemPawnRookAreaParams[gGameSession->location.loc.stage];
            if (areaSoundSets != NULL) {
                work->soundSet = areaSoundSets[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                soundFileKey[3]  = 0;
                soundFileKey[2]  = GOLEM_PAWN_ROOK_SOUND_FILE_GROUP;
                soundFileKey[0]  = work->soundSet;
                soundFileArgs[0] = GOLEM_PAWN_ROOK_ID;
                soundFileArgs[3] = 0;
                soundFileArgs[2] = 0;
                soundFileArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, soundFileKey, soundFileArgs);
            }

#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
            work->shieldHp = GOLEM_PAWN_ROOK_SHIELD_HP;
#endif
            // Sight, hurt, floor, weapon and laser bodies borrow the owned contact arrays.
            work->sightCapsule.ends[0].vz = GOLEM_PAWN_ROOK_SIGHT_LENGTH;
            work->sightCapsule.end0Radius = GOLEM_PAWN_ROOK_SIGHT_FAR_RADIUS;
            work->sightCapsule.ends[0].vx = 0;
            work->sightCapsule.ends[0].vy = 0;
            work->sightCapsule.ends[1].vx = 0;
            work->sightCapsule.ends[1].vy = 0;
            work->sightCapsule.ends[1].vz = 0;
            work->sightCapsule.end1Radius = GOLEM_PAWN_ROOK_SIGHT_NEAR_RADIUS;
            work->sightCapsule.contacts   = work->sightContacts;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
            randomDraw          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->screamCharges = ((randomDraw >> 16) & 1) + 1;
            gRandomLcgState     = randomDraw;
#endif
            sightCoords                     = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &sightCoords[GOLEM_PAWN_ROOK_SIGHT_PART];
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody);
            worldCollisionInitContacts(work->sightContacts, ARRAY_SIZE(work->sightContacts), 0);
            work->sightBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            hurtCoords                      = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GOLEM_PAWN_ROOK_ID;
            work->hurtBody.radius           = GOLEM_PAWN_ROOK_HURT_RADIUS;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &hurtCoords[GOLEM_PAWN_ROOK_BODY_PART];
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody);
            worldCollisionInitContacts(work->hurtContacts, ARRAY_SIZE(work->hurtContacts), 0);
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

            groundCoords                      = actor->extra.tmd->coords;
            work->groundBody.pos.vy           = -GOLEM_PAWN_ROOK_GROUND_RADIUS;
            work->groundBody.context.contacts = work->groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0;
            work->groundBody.radius           = GOLEM_PAWN_ROOK_GROUND_RADIUS;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->groundBody.coord            = groundCoords;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody);
            worldCollisionInitContacts(work->groundContacts, ARRAY_SIZE(work->groundContacts), 0);
            work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            weaponCoords                      = weaponEnemy->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = GOLEM_PAWN_ROOK_STRIKE_OFFSET_Y;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = GOLEM_PAWN_ROOK_STRIKE_RADIUS;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = weaponCoords;
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
            laserCoords                     = actor->extra.tmd->coords;
            work->laserBody.context.capsule = &work->laserCapsule;
            work->laserBody.pos.vx          = 0;
            work->laserBody.pos.vy          = 0;
            work->laserBody.pos.vz          = 0;
            work->laserBody.key             = 0;
            work->laserBody.radius          = 0;
            work->laserBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->laserBody.coord           = laserCoords;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->laserBody);
            worldCollisionInitContacts(work->laserContacts, ARRAY_SIZE(work->laserContacts), 0);
            work->laserBody.flags = (work->laserBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            actor->state          = GOLEM_PAWN_ROOK_TASK_RUNNING;
            break;
        case GOLEM_PAWN_ROOK_DOWNED_BEHIND:
            work->anim   = GOLEM_PAWN_ROOK_CORPSE_BEHIND_ANIM;
            work->step   = GOLEM_PAWN_ROOK_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
        case GOLEM_PAWN_ROOK_DOWNED_FRONT:
            work->anim   = GOLEM_PAWN_ROOK_CORPSE_FRONT_ANIM;
            work->step   = GOLEM_PAWN_ROOK_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
    }
}
