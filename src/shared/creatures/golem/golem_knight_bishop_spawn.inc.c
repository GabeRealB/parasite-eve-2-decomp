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
    animationInitContext(&work->rig.anim, _gGolemKnightBishopAnimSets, model, work->rig.poses, work->rig.slots);
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
            enemy->param      = &_gGolemKnightBishopParams;
            enemy->recs       = work->hurtContacts;
            enemy->hp         = _gGolemKnightBishopParams.hpMax;
            for (roomIndex = 0; _gGolemKnightBishopRoomRegions[roomIndex].regionTable != GOLEM_KNIGHT_BISHOP_REGION_TABLE_END; roomIndex++) {
                if (gGameSession->location.stage == _gGolemKnightBishopRoomRegions[roomIndex].stage && gGameSession->location.area == _gGolemKnightBishopRoomRegions[roomIndex].area) {
                    work->regions     = _gGolemKnightBishopRegionTables[_gGolemKnightBishopRoomRegions[roomIndex].regionTable];
                    work->regionCount = _gGolemKnightBishopRoomRegions[roomIndex].regionCount;
                }
            }
            work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN;
            (sceneAcquireBattleRef)(0);
            work->actorId = GOLEM_KNIGHT_BISHOP_ACTOR_ID;
            areaSoundSets = _gGolemKnightBishopStageSoundSets[gGameSession->location.stage];
            if (areaSoundSets != NULL) {
                work->soundSet = areaSoundSets[gGameSession->location.area];
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
            work->hurtBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GOLEM_KNIGHT_BISHOP_ACTOR_ID;
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
            work->groundBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GOLEM_KNIGHT_BISHOP_ACTOR_ID;
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
            work->strikeBody.key              = damagePackAttackKey(_gGolemKnightBishopAttacks, GOLEM_KNIGHT_BISHOP_ATTACK_STRIKE);
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
            task->msgTable          = _gGolemKnightBishopMessages;
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

/// Updates the active GOLEM's combat behaviour, animation and presentation.
///
/// `enemy` belongs to `task`, whose model and GOLEM work are initialized.
/// Running mode derives target eligibility from the hurt body's pair-enable
/// bit. Paused mode draws tint/shadow without advancing behaviour; hidden mode
/// suppresses drawing and targeting. A missing room-region table skips updates.
/// Motion and animation precede sound cues, composition and appearance updates;
/// the refracted-frame capture uses part 3's depth with the shared depth bias.
static void _golemKnightBishopFrameState(Enemy* enemy, Task* task)
{
    GolemKnightBishopWork* work;
    TmdObject*             model;
    GfxCoord*              root;

    work  = task->work;
    model = task->extra.tmd;
    root  = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->fadeState != GOLEM_KNIGHT_BISHOP_FADE_HIDDEN) {
                model->flags = 0;
            }
            enemy->node.state.parts.flags = (work->hurtBody.flags & WORLD_COLLISION_BODY_PAIR_ENABLED) ? 0 : WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _golemKnightBishopUpdateTint(task);
            _golemKnightBishopDrawShadow(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->regions != NULL) {
        // Resolve reactions and movement before advancing the requested animation.
        _golemKnightBishopTakeHits(task);
        _golemKnightBishopRunSequence(task);
        _golemKnightBishopStepForward(task);
        _golemKnightBishopTickAnim(task);
        _golemKnightBishopPlayAnimCues(task);
        root->composeStamp                      = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(root);
        _golemKnightBishopUpdateTint(task);
        _golemKnightBishopDrawShadow(task);
        _golemKnightBishopQueueFrameCapture(&task->extra.tmd->coords[3], GOLEM_KNIGHT_BISHOP_FRAME_CAPTURE_BIAS);
        _golemKnightBishopUpdateAppearance(task);
        modelLightingSetLayerMaterials(work->translucency);
    }
}

/// Dispatches one GOLEM sequence update and advances player-release bookkeeping.
///
/// `task` owns initialized GOLEM work/model data. Sequences 0..11 select the
/// shared behaviour handlers; other values dispatch none. The release update
/// tests the sequence after the handler may have changed it. A retained grab
/// sequence advances release only on its own release paths.
static void _golemKnightBishopRunSequence(Task* task)
{
    GolemKnightBishopWork* work;

    work = task->work;
    switch (work->sequence) {
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE:
            _golemKnightBishopIdleSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB:
            _golemKnightBishopGrabSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE:
            _golemKnightBishopStrikeSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH:
            _golemKnightBishopBoxApproachSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER:
            _golemKnightBishopRecoverSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH:
            _golemKnightBishopLightFlinchSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH:
            _golemKnightBishopHeavyFlinchSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL:
            _golemKnightBishopKnockdownSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT:
            _golemKnightBishopDownedHitSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH:
            _golemKnightBishopCollapseDeathSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH:
            _golemKnightBishopDownedDeathSeq(task);
            break;
        case GOLEM_KNIGHT_BISHOP_SEQUENCE_REGION_SCAN:
            _golemKnightBishopRegionScanSeq(task);
            break;
    }
    // Re-read the sequence: release may begin or end during the handler above.
    if (work->sequence != GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
        _golemKnightBishopTickGrabRelease(task);
    }
}

/// Rebuilds the feint's root transform with its current per-axis scale.
///
/// `task` owns a live GOLEM model and work block. The saved unscaled transform
/// supplies the translation and rotation; `scale` uses 4096 for unity. One
/// scratch-stack matrix is reserved and released, without composing the root.
static void _golemKnightBishopApplyScale(Task* task)
{
    void**                 cursorSlot;
    MATRIX*                scaleMatrix;
    GfxCoord*              root;
    GolemKnightBishopWork* work;

    cursorSlot  = SCRATCH_STACK_CURSOR_SLOT;
    scaleMatrix = SCRATCH_STACK_RESERVE_BLOCK_AT(cursorSlot, MATRIX);
    root        = &task->extra.tmd->coords[0];
    work        = task->work;

    root->coord = work->unscaledRootMtx;
    gfxSetRotIdentity(scaleMatrix);
    ScaleMatrix(scaleMatrix, &work->scale);
    MulMatrix(&root->coord, scaleMatrix);
    SCRATCH_STACK_RELEASE_BLOCK_AT(cursorSlot, MATRIX);
}

/// Plays a fatal hit from the saved downed pose and enters the corpse state.
///
/// `task` owns a live GOLEM model/work block. The reaction lasts 16 frames
/// after a hit from behind, 22 after one from the front. It requests an
/// appearance fade with nominal lengths of ten translucency frames and five
/// colour-blend frames, then hands the task to its dead-state handler.
static void _golemKnightBishopDownedDeathSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START            = 0,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND           = 1,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT            = 2,
        GOLEM_KNIGHT_BISHOP_TASK_DEAD                        = 2,
        GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_TRANSLUCENCY_FRAMES = 10,
        GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_COLOR_FRAMES        = 5,
    };
    GolemKnightBishopWork* work;
    s16                    step;
    s32                    downedPose;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START:
            // Keep the widened pose snapshot that also selects the reaction step.
            downedPose = work->downedPose;
            if (downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT;
                work->step = GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT;
            }
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_TRANSLUCENCY_FRAMES;
            work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_COLOR_FRAMES;
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_BEHIND_FRAMES) {
                task->state = GOLEM_KNIGHT_BISHOP_TASK_DEAD;
                work->step  = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_FRONT_FRAMES) {
                task->state = step;
                work->step  = 0;
            }
            break;
    }
}

/// Saves the root position and advances it along the GOLEM's horizontal facing.
///
/// `forwardSpeed` is a signed world distance per frame; the root basis has
/// twelve fractional bits. While upright or starting to fall, a downward
/// floor-query displacement is added. Collision rejection can restore
/// `prevRootPos`.
static void _golemKnightBishopStepForward(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BASIS_FRACTION_BITS = 12,
        GOLEM_KNIGHT_BISHOP_FLOOR_QUERY_STEP    = 128,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALLING   = 2,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;

    root                 = &task->extra.tmd->coords[0];
    work                 = task->work;
    work->prevRootPos.vx = root->coord.t[0];
    work->prevRootPos.vy = root->coord.t[1];
    work->prevRootPos.vz = root->coord.t[2];
    root->coord.t[0]    += (root->coord.m[0][2] * work->forwardSpeed) >> GOLEM_KNIGHT_BISHOP_BASIS_FRACTION_BITS;
    if (work->knockdownStage < GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALLING) {
        root->coord.t[1] += GOLEM_KNIGHT_BISHOP_FLOOR_QUERY_STEP;
    }
    root->coord.t[2] += (root->coord.m[2][2] * work->forwardSpeed) >> GOLEM_KNIGHT_BISHOP_BASIS_FRACTION_BITS;
}

/// Applies a changed GOLEM animation request or advances the active animation.
///
/// `task` owns an initialized nineteen-part rig. Requested clip IDs 1..21
/// select live entries in the carrier's animation and blend-duration tables;
/// animation entry 0 is empty. A changed ID
/// restarts slots 1..18 with the selected duration in frames and clears
/// `animFrame`; an unchanged ID increments that signed halfword and ticks the
/// same slots once. Root slot 0 is left alone.
static void _golemKnightBishopTickAnim(Task* task)
{
    _golemKnightBishopTickAnimInline(task);
}

/// Refreshes the GOLEM's room lighting and consumes its pending ambient tint.
///
/// `task` owns a live model, GOLEM work block and Enemy spawn argument. The root's
/// composed world position must be current; no coordinates are composed here.
static void _golemKnightBishopUpdateTint(Task* task)
{
    _golemKnightBishopUpdateTintInline(task);
}

/// Draws the GOLEM's ground shadow from its already-composed coordinates.
///
/// Uses part 3's world X/Z and the root's world height, with a half-side of
/// 768 world units. A zero shade becomes the negative no-shadow sentinel;
/// negative shades suppress drawing. `task` owns a live GOLEM model/work block.
static void _golemKnightBishopDrawShadow(Task* task)
{
    _golemKnightBishopDrawShadowInline(task);
}

/// Advances the grab release cue and returns scripted control to the player.
///
/// With a live player task and nonzero grab stage, plays the cue at stored tick
/// 20, then increments the signed halfword counter. From incremented tick 95,
/// an animation-complete reply ends scripted control and clears the grab stage.
/// The sequence dispatcher skips the grab itself, whose release paths call this.
static void _golemKnightBishopTickGrabRelease(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_CUE_TICK  = 20,
        GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_MIN_TICKS = 95,
    };
    GolemKnightBishopWork* work;
    s16                    releaseTimer;
    s32                    sound;
    s32                    audioPan;
    Task*                  player;
    GfxCoord*              root;

    work   = task->work;
    root   = task->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->grabStage != GOLEM_KNIGHT_BISHOP_GRAB_NONE) {
        if (work->grabReleaseTimer == GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_CUE_TICK) {
            sound    = _gGolemKnightBishopGrabReleaseSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            audioPan = (s8)worldCoordGetOriginAudioPan(root);
            sndEvtRequestScriptStart(sound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
        }
        releaseTimer           = work->grabReleaseTimer + 1;
        work->grabReleaseTimer = releaseTimer;
        if ((releaseTimer >= GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_MIN_TICKS) && (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->grabStage = GOLEM_KNIGHT_BISHOP_GRAB_NONE;
        }
    }
}
