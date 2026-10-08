/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Initializes a live Knight/Bishop GOLEM or restores its saved corpse pose.
///
/// `enemy` and its model task remain live for the work block's lifetime; allocation
/// failure destroys both through the enemy lifecycle. Fresh setup borrows the
/// carrier's animation/region tables and links six collision bodies, including
/// two player-parented placement probes sharing one contact record. Saved poses
/// 1 and 2 select the rear and front lying clips and the corpse handler.
static void _golemKnightBishopSpawn(Enemy* enemy, Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_SPAWN_FRESH = 0,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_SOUND_FILE_GROUP    = 40,
        GOLEM_KNIGHT_BISHOP_SOUND_FILE_HUNDREDS = 22,
    };
    u8                     fileKey[4];
    u8                     loadArgs[4];
    GolemKnightBishopWork* work;
    TmdObject*             model;
    GfxCoord*              root;
    s16*                   areaSoundSets;
    WorldCollisionContact* hurtContacts;
    WorldCollisionContact* groundContacts;
    WorldCollisionContact* strikeContacts;
    WorldCollisionContact* probeContacts;
    WorldCollisionContact* aimContacts;
    s32                    slotIndex;
    s32                    roomIndex;
    s32                    spawnPose;

    model = task->extra.tmd;
    root  = model->coords;
    work  = memCalloc(sizeof(GolemKnightBishopWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                    = work;
    model->flags                  = 0;
    root->composeStamp            = GRAPHICS_COORD_DIRTY;
    model->lightMtx               = &work->lightMtx;
    model->colorMtx               = &work->colorMtx;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, gGolemKnightBishopAnimSets, model, work->rig.poses, work->rig.slots);
    work->anim        = GOLEM_KNIGHT_BISHOP_ANIM_RECOVER;
    work->playingAnim = GOLEM_KNIGHT_BISHOP_ANIM_RECOVER;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, work->anim);
    }
    spawnPose = enemy->spawnState;
    switch (spawnPose) {
        case GOLEM_KNIGHT_BISHOP_SPAWN_FRESH:
            work->translucency        = GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE;
            model->shading.colorBlend = 0;
            modelLightingSetLayerMaterials(work->translucency);
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            enemy->field_4    = &root->coord;
            enemy->field_48   = 0;
            worldTargetLinkNode(&enemy->node);
            enemy->coord      = &task->extra.tmd->coords[3];
            enemy->bodyPos.vx = 0;
            enemy->bodyPos.vy = 0;
            enemy->bodyPos.vz = 0;
            enemy->param      = &gGolemKnightBishopParams;
            enemy->recs       = work->hurtContacts;
            enemy->hp         = gGolemKnightBishopParams.hpMax;
            for (roomIndex = 0; gGolemKnightBishopSpots[roomIndex].regionTable != 0; roomIndex++) {
                if (gGameSession->location.loc.stage == gGolemKnightBishopSpots[roomIndex].stage && gGameSession->location.loc.area == gGolemKnightBishopSpots[roomIndex].area) {
                    work->regions     = gGolemKnightBishopRegions[gGolemKnightBishopSpots[roomIndex].regionTable];
                    work->regionCount = gGolemKnightBishopSpots[roomIndex].regionCount;
                }
            }
            work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN;
            (sceneAcquireBattleRef)(0);
            work->actorId = GOLEM_KNIGHT_BISHOP_ID;
            areaSoundSets = gGolemKnightBishopStageCues[gGameSession->location.loc.stage];
            if (areaSoundSets != NULL) {
                work->soundSet = areaSoundSets[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                // CD dispatch reads key bytes 3, 2 and 0; byte 1 is ignored.
                fileKey[3]  = 0;
                fileKey[2]  = GOLEM_KNIGHT_BISHOP_SOUND_FILE_GROUP;
                fileKey[0]  = work->soundSet;
                loadArgs[0] = GOLEM_KNIGHT_BISHOP_SOUND_FILE_HUNDREDS;
                loadArgs[3] = 0;
                loadArgs[2] = 0;
                loadArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
            }
            // Bind the live body/contact storage before adding each body to its list.
            work->hurtBody.coord            = &task->extra.tmd->coords[3];
            hurtContacts                    = work->hurtContacts;
            work->hurtBody.context.contacts = hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GOLEM_KNIGHT_BISHOP_ID;
            work->hurtBody.radius           = GOLEM_KNIGHT_BISHOP_HURT_RADIUS;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody);
            worldCollisionInitContacts(hurtContacts, ARRAY_SIZE(work->hurtContacts), 0);
            work->hurtBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->groundBody.coord            = task->extra.tmd->coords;
            groundContacts                    = work->groundContacts;
            work->groundBody.context.contacts = groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vy           = -0x1F4;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GOLEM_KNIGHT_BISHOP_ID;
            work->groundBody.radius           = 0x1F4;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody);
            worldCollisionInitContacts(groundContacts, ARRAY_SIZE(work->groundContacts), 0);
            work->groundBody.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->strikeBody.coord            = &task->extra.tmd->coords[8];
            strikeContacts                    = work->strikeContacts;
            work->strikeBody.context.contacts = strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = 0;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = damagePackAttackKey(gGolemKnightBishopAttacks, 1);
            work->strikeBody.radius           = 0x12C;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody);
            worldCollisionInitContacts(strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
            work->strikeBody.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->pathProbeCapsule.ends[0].vx   = 0;
            work->pathProbeCapsule.ends[0].vy   = -0x3E8;
            work->pathProbeCapsule.ends[0].vz   = -0x7D0;
            work->pathProbeCapsule.ends[1].vx   = 0;
            work->pathProbeCapsule.ends[1].vy   = -0x3E8;
            work->pathProbeCapsule.ends[1].vz   = 0;
            work->pathProbeCapsule.end0Radius   = 0x1F4;
            work->pathProbeCapsule.end1Radius   = 0x1F4;
            probeContacts                       = work->probeContacts;
            work->pathProbeCapsule.contacts     = probeContacts;
            work->pathProbeBody.coord           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->pathProbeBody.context.capsule = &work->pathProbeCapsule;
            work->pathProbeBody.pos.vx          = 0;
            work->pathProbeBody.pos.vy          = 0;
            work->pathProbeBody.pos.vz          = 0;
            work->pathProbeBody.key             = 0;
            work->pathProbeBody.radius          = 0;
            work->pathProbeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->pathProbeBody);
            worldCollisionInitContacts(probeContacts, ARRAY_SIZE(work->probeContacts), 0);
            work->pathProbeBody.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->spotProbeBody.coord            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->spotProbeBody.context.contacts = probeContacts;
            work->spotProbeBody.pos.vx           = 0;
            work->spotProbeBody.pos.vy           = -0x320;
            work->spotProbeBody.pos.vz           = -GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
            work->spotProbeBody.key              = 0;
            work->spotProbeBody.radius           = 0x1F4;
            work->spotProbeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->spotProbeBody);
            work->aimBeamCapsule.ends[0].vx   = 0;
            work->aimBeamCapsule.ends[0].vy   = GOLEM_KNIGHT_BISHOP_AIM_ROOT_Y;
            work->aimBeamCapsule.ends[0].vz   = GOLEM_KNIGHT_BISHOP_AIM_RANGE;
            work->aimBeamCapsule.ends[1].vx   = 0;
            work->aimBeamCapsule.ends[1].vy   = 0;
            work->aimBeamCapsule.ends[1].vz   = 0;
            work->aimBeamCapsule.end0Radius   = 1;
            work->aimBeamCapsule.end1Radius   = 1;
            aimContacts                       = work->aimBeamContacts;
            work->aimBeamCapsule.contacts     = aimContacts;
            work->aimBeamBody.coord           = root;
            work->aimBeamBody.context.capsule = &work->aimBeamCapsule;
            work->aimBeamBody.pos.vx          = 0;
            work->aimBeamBody.pos.vy          = 0;
            work->aimBeamBody.pos.vz          = 0;
            work->aimBeamBody.key             = 0;
            work->aimBeamBody.radius          = 0;
            work->aimBeamBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->spotProbeBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->aimBeamBody);
            worldCollisionInitContacts(aimContacts, ARRAY_SIZE(work->aimBeamContacts), 0);
            work->aimBeamBody.flags = (work->aimBeamBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            task->msgTable          = gGolemKnightBishopMessages;
            task->state             = GOLEM_KNIGHT_BISHOP_TASK_ACTIVE;
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND:
            work->anim                = GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND;
            work->step                = 2;
            task->state               = GOLEM_KNIGHT_BISHOP_TASK_DEAD;
            work->translucency        = 0;
            model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            modelLightingSetLayerMaterials(work->translucency);
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_FRONT:
            work->anim                = GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT;
            work->step                = spawnPose;
            task->state               = spawnPose;
            work->translucency        = 0;
            model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            modelLightingSetLayerMaterials(work->translucency);
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            break;
    }
}
