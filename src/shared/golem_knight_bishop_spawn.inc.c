/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Spawn handler. Allocates the work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `spawnState`. State 0 is the full setup: it links the
/// enemy node, picks `regions` and `regionCount` for the current stage / room
/// out of `gGolemKnightBishopSpots`, requests the room's sound file, and links
/// the work block's six collision bodies with their contact tables before
/// moving the task on (`Task::state` 1). States 1 and 2 restore a dead golem,
/// lying as the `downedPose` filed at its death.
void golemKnightBishopSpawn(Enemy* arg0, Task* arg1)
{
    u8                     param1[4];
    u8                     param2[4];
    GolemKnightBishopWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    s16*                   cues;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    WorldCollisionContact* records4;
    WorldCollisionContact* records5;
    s32                    i;
    s32                    kind;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(GolemKnightBishopWork), 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work                    = work;
    obj->flags                    = 0;
    coord->composeStamp           = GRAPHICS_COORD_DIRTY;
    obj->lightMtx                 = &work->lightMtx;
    obj->colorMtx                 = &work->colorMtx;
    work->hitEffectArg.coord      = &arg1->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, gGolemKnightBishopAnimSets, obj, work->rig.poses, work->rig.slots);
    work->anim        = 0xB;
    work->playingAnim = 0xB;
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, work->anim);
    }
    kind = arg0->spawnState;
    switch (kind) {
        case 0:
            work->translucency      = 0xFF;
            obj->shading.colorBlend = 0;
            func_8009EA50(work->translucency);
            work->shadowShade = -1;
            arg0->field_4     = &coord->coord;
            arg0->field_48    = 0;
            Gp_LinkNode(&arg0->node);
            arg0->coord      = &arg1->extra.tmd->coords[3];
            arg0->bodyPos.vx = 0;
            arg0->bodyPos.vy = 0;
            arg0->bodyPos.vz = 0;
            arg0->param      = &gGolemKnightBishopParams;
            arg0->recs       = work->hurtContacts;
            arg0->hp         = gGolemKnightBishopParams.hpMax;
            for (i = 0; gGolemKnightBishopSpots[i].regionTable != 0; i++) {
                if (gGameSession->location.loc.stage == gGolemKnightBishopSpots[i].stage && gGameSession->location.loc.area == gGolemKnightBishopSpots[i].area) {
                    work->regions     = gGolemKnightBishopRegions[gGolemKnightBishopSpots[i].regionTable];
                    work->regionCount = gGolemKnightBishopSpots[i].regionCount;
                }
            }
            work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN;
            (Gp_IncStateF0Ref)(0);
            work->actorId = GOLEM_KNIGHT_BISHOP_ID;
            cues          = gGolemKnightBishopStageCues[gGameSession->location.loc.stage];
            if (cues != NULL) {
                work->soundSet = cues[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                param1[3] = 0;
                param1[2] = 0x28;
                param1[0] = work->soundSet;
                param2[0] = 0x16;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            }
            work->hurtBody.coord            = &arg1->extra.tmd->coords[3];
            records1                        = work->hurtContacts;
            work->hurtBody.context.contacts = records1;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = 0x30000 | GOLEM_KNIGHT_BISHOP_ID;
            work->hurtBody.radius           = 0x15E;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(2, &work->hurtBody);
            Gp_InitRec18Table(records1, ARRAY_SIZE(work->hurtContacts), 0);
            work->hurtBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->groundBody.coord            = arg1->extra.tmd->coords;
            records2                          = work->groundContacts;
            work->groundBody.context.contacts = records2;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vy           = -0x1F4;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0x30000 | GOLEM_KNIGHT_BISHOP_ID;
            work->groundBody.radius           = 0x1F4;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(2, &work->groundBody);
            Gp_InitRec18Table(records2, ARRAY_SIZE(work->groundContacts), 0);
            work->groundBody.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->strikeBody.coord            = &arg1->extra.tmd->coords[8];
            records3                          = work->strikeContacts;
            work->strikeBody.context.contacts = records3;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = 0;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = Gp_PackPair(gGolemKnightBishopAttacks, 1);
            work->strikeBody.radius           = 0x12C;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(3, &work->strikeBody);
            Gp_InitRec18Table(records3, ARRAY_SIZE(work->strikeContacts), 0);
            work->strikeBody.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->pathProbeCapsule.ends[0].vx   = 0;
            work->pathProbeCapsule.ends[0].vy   = -0x3E8;
            work->pathProbeCapsule.ends[0].vz   = -0x7D0;
            work->pathProbeCapsule.ends[1].vx   = 0;
            work->pathProbeCapsule.ends[1].vy   = -0x3E8;
            work->pathProbeCapsule.ends[1].vz   = 0;
            work->pathProbeCapsule.end0Radius   = 0x1F4;
            work->pathProbeCapsule.end1Radius   = 0x1F4;
            records4                            = work->probeContacts;
            work->pathProbeCapsule.contacts     = records4;
            work->pathProbeBody.coord           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->pathProbeBody.context.capsule = &work->pathProbeCapsule;
            work->pathProbeBody.pos.vx          = 0;
            work->pathProbeBody.pos.vy          = 0;
            work->pathProbeBody.pos.vz          = 0;
            work->pathProbeBody.key             = 0;
            work->pathProbeBody.radius          = 0;
            work->pathProbeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            Gp_LinkObj(3, &work->pathProbeBody);
            Gp_InitRec18Table(records4, ARRAY_SIZE(work->probeContacts), 0);
            work->pathProbeBody.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->spotProbeBody.coord            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->spotProbeBody.context.contacts = records4;
            work->spotProbeBody.pos.vx           = 0;
            work->spotProbeBody.pos.vy           = -0x320;
            work->spotProbeBody.pos.vz           = -0x5AA;
            work->spotProbeBody.key              = 0;
            work->spotProbeBody.radius           = 0x1F4;
            work->spotProbeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(3, &work->spotProbeBody);
            work->aimBeamCapsule.ends[0].vx   = 0;
            work->aimBeamCapsule.ends[0].vy   = -0x514;
            work->aimBeamCapsule.ends[0].vz   = 0x2710;
            work->aimBeamCapsule.ends[1].vx   = 0;
            work->aimBeamCapsule.ends[1].vy   = 0;
            work->aimBeamCapsule.ends[1].vz   = 0;
            work->aimBeamCapsule.end0Radius   = 1;
            work->aimBeamCapsule.end1Radius   = 1;
            records5                          = work->aimBeamContacts;
            work->aimBeamCapsule.contacts     = records5;
            work->aimBeamBody.coord           = coord;
            work->aimBeamBody.context.capsule = &work->aimBeamCapsule;
            work->aimBeamBody.pos.vx          = 0;
            work->aimBeamBody.pos.vy          = 0;
            work->aimBeamBody.pos.vz          = 0;
            work->aimBeamBody.key             = 0;
            work->aimBeamBody.radius          = 0;
            work->aimBeamBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->spotProbeBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_LinkObj(3, &work->aimBeamBody);
            Gp_InitRec18Table(records5, ARRAY_SIZE(work->aimBeamContacts), 0);
            work->aimBeamBody.flags = (work->aimBeamBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            arg1->msgTable          = gGolemKnightBishopMessages;
            arg1->state             = 1;
            break;
        case 1:
            work->anim              = 0x10;
            work->step              = 2;
            arg1->state             = 2;
            work->translucency      = 0;
            obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            func_8009EA50(work->translucency);
            work->shadowShade = 0x80;
            break;
        case 2:
            work->anim              = 0x14;
            work->step              = kind;
            arg1->state             = kind;
            work->translucency      = 0;
            obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            func_8009EA50(work->translucency);
            work->shadowShade = 0x80;
            break;
    }
}
