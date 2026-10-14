/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Captures slots 1..8 and starts a blend into their requested clip.
///
/// Sets each slot's rate from animRate, narrowed to its signed low byte in
/// sixteenths of a frame, then captures the slot's current ticked pose and seeks
/// record zero of animId. animBlendFrames counts whole normal-rate frames;
/// 0..2047 keeps the playback API's signed time nonnegative. Requires an
/// initialized nine-slot context, live pose storage and valid loaded clip tracks.
/// Slot zero and the request latches are retained; the caller owns all storage.
static inline void _madChaserBlendAnimSlots(MadChaserWork* requestWork)
{
    s32 blendSlot;

    for (blendSlot = 1; blendSlot < ARRAY_SIZE(requestWork->slots); blendSlot++) {
        requestWork->slots[blendSlot].rate = requestWork->animRate;
        animationSeekSlotWithBlend(&requestWork->anim, blendSlot, requestWork->animId, 0, requestWork->animBlendFrames);
    }
}

/// Applies animation requests and advances the eight animated model parts.
///
/// Slots 1..8 share animId and the signed low byte of animRate (sixteenths of a
/// frame); slot 0 is retained. A blend seeks even when the clip repeats, and clears
/// animBlendFrames only after changing clips. A reset cuts to the requested clip.
/// Either request becomes PLAYING with animFrames zero; later PLAYING calls
/// increment that 16-bit counter. Every call ticks the eight slots, including an
/// idle request value of zero. Requires initialized nine-slot/pose storage, a
/// nine-part model, a valid loaded animation-bank entry, and the playback API's
/// scratch-stack and track-data requirements.
static void _madChaserTickAnim(Task* task)
{
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            resetSlot;
    s32            playSlot;

    work = task->work;
    // Apply requests before ticking so a blend captures the preceding pose.
    if (work->animRequest == MAD_CHASER_ANIM_REQUEST_BLEND) {
        requestWork = work;
        if (requestWork->appliedAnim == requestWork->animId) {
            _madChaserBlendAnimSlots(requestWork);
        } else {
            _madChaserBlendAnimSlots(requestWork);
            requestWork->animBlendFrames = 0;
        }
        requestWork->appliedAnim = requestWork->animId;
        work->animRequest        = MAD_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames         = 0;
    } else if (work->animRequest == MAD_CHASER_ANIM_REQUEST_RESET) {
        requestWork = work;
        for (resetSlot = 1; resetSlot < ARRAY_SIZE(requestWork->slots); resetSlot++) {
            animationResetSlot(&requestWork->anim, resetSlot, requestWork->animId);
            requestWork->slots[resetSlot].rate = requestWork->animRate;
        }
        requestWork->appliedAnim = requestWork->animId;
        work->animRequest        = MAD_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames         = 0;
    } else if (work->animRequest == MAD_CHASER_ANIM_REQUEST_PLAYING) {
        work->animFrames++;
    }
    // A newly applied request also advances its first pose on this call.
    for (playSlot = 1; playSlot < ARRAY_SIZE(work->slots); playSlot++) {
        work->slots[playSlot].rate = work->animRate;
        animationTickSlot(&work->anim, playSlot);
    }
}

/// Attaches a sphere at model part 1's origin with borrowed contact storage.
///
/// taskArg is a live Task*, sphereArg a writable WorldCollisionBody* and
/// contactsArg a live WorldCollisionContact* table retained until unlinking.
/// taskArg and contactsArg are evaluated once; sphereArg five times. Supply
/// stable expressions without side effects. Expands to one compound statement.
#define MAD_CHASER_SET_SPHERE_ORIGIN(taskArg, sphereArg, contactsArg)     \
    {                                                                     \
        (sphereArg)->coord            = &(taskArg)->extra.tmd->coords[1]; \
        (sphereArg)->context.contacts = (contactsArg);                    \
        (sphereArg)->pos.vx           = 0;                                \
        (sphereArg)->pos.vy           = 0;                                \
        (sphereArg)->pos.vz           = 0;                                \
    }

/// Initializes and links the Mad Chaser's hit, attack and room-grid spheres.
///
/// All three borrow model part 1's coordinate and work-owned contact storage.
/// The 368-unit hit and attack spheres start with pair tests on and off,
/// respectively; the 548-unit grid sphere has only grid tests enabled and shares
/// the hit sphere's eight contacts. The attack uses enemy attack definition 0
/// and its own two contacts. Requires zeroed live work, a nine-part model and
/// an Enemy spawn argument; unlink every sphere before freeing either owner.
static void _madChaserLinkBodies(Task* task)
{
    enum {
        MAD_CHASER_BODY_ID     = 0x2C,
        MAD_CHASER_PAIR_RADIUS = 368,
        MAD_CHASER_GRID_RADIUS = 548,
    };
    MadChaserWork* work = task->work;

    MAD_CHASER_SET_SPHERE_ORIGIN(task, &work->pairBody, work->contacts);
    work->pairBody.key    = WORLD_COLLISION_CONTACT_ENEMY_BODY | MAD_CHASER_BODY_ID;
    work->pairBody.radius = MAD_CHASER_PAIR_RADIUS;
    work->pairBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->pairBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->pairBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    MAD_CHASER_SET_SPHERE_ORIGIN(task, &work->attackBody, work->attackContacts);
    work->attackBody.key    = damagePackEnemyAttackKey(task->spawnArg2.pointer, 0);
    work->attackBody.radius = MAD_CHASER_PAIR_RADIUS;
    work->attackBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    MAD_CHASER_SET_SPHERE_ORIGIN(task, &work->gridBody, work->contacts);
    work->gridBody.key    = WORLD_COLLISION_CONTACT_ENEMY_BODY | MAD_CHASER_BODY_ID;
    work->gridBody.radius = MAD_CHASER_GRID_RADIUS;
    work->gridBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#undef MAD_CHASER_SET_SPHERE_ORIGIN

/// Behaviors of ordinary death, including the alternate blast-burst path.
///
/// `MadChaserWork::state` selects 0 release the target, 1 settle, 2 wait for animation, 3
/// start shrink, 4 turn translucent, 5 shrink with burn, 6 start despawn, 7 pause, 8 burst.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable9 _gMadChaserDeathStates = { {
    _madChaserDeathReleaseTarget,
    _madChaserDeathSettle,
    _madChaserDeathWaitAnim,
    _madChaserDeathStartShrink,
    _madChaserDeathTurnTranslucent,
    _madChaserShrinkWithBurn,
    _madChaserStartDespawn,
    _madChaserDeathPause,
    _madChaserBurst,
} };

/// Runs one ordinary-death frame and refreshes colour and visible limb shadows.
///
/// Requires live enemy/model/work storage with at least nine model coordinates
/// and work->state in 0..8. Hidden actors skip drawing and all frame work.
/// Running actors increment the wrapping s16 frame counter, dispatch one copied
/// death handler and dirty the root. Running and paused actors both sample
/// colour at part 1 and draw three shadows unless shadowHidden is set. Retains
/// the model's draw-skip flag when actors resume.
static void _madChaserDeathTick(Task* task)
{
    TmdObject*     model  = task->extra.tmd;
    MadChaserWork* work   = task->work;
    GfxCoord*      root   = model->coords;
    TaskFuncTable9 states = _gMadChaserDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states.funcs[(s16)work->state](task);
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            // Paused frames retain presentation updates without advancing death.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(task, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Shrinks the ordinary corpse on Y while starting its burn effect.
///
/// Requires live enemy/model/work in ordinary-death behavior 5, with the saved
/// root transform, Q12 Y scale and zeroed stateFrames prepared by death entry.
/// Subtracts 1/64 from the u16 scale each updating frame and rescales a fresh
/// copy of the saved matrix. At count 4 starts three burn bursts at the root;
/// at 16 selects black colour, and after 32 hides the model and advances behavior.
/// Frame comparisons interpret the wrapping u16 counter as s16. Storage stays
/// live; the death dispatcher dirties the root and updates colour afterward.
static void _madChaserShrinkWithBurn(Task* task)
{
    enum {
        MAD_CHASER_DEATH_SHRINK_SCALE_STEP = ONE / 64,
        MAD_CHASER_DEATH_BURN_START_FRAME  = 4,
        MAD_CHASER_DEATH_BURN_BURSTS       = 3,
        MAD_CHASER_DEATH_BLACK_FRAME       = 16,
        MAD_CHASER_DEATH_HIDE_AFTER_FRAME  = 32
    };
    MadChaserWork* work;
    TmdObject*     model;
    GfxCoord*      root;
    VECTOR         scale;
    MATRIX         shrinkMatrix;
    SVECTOR        originOffset;

    work  = task->work;
    model = task->extra.tmd;
    root  = model->coords;
    // Rescale the saved pose so rounding never accumulates across frames.
    work->shrinkScaleY -= MAD_CHASER_DEATH_SHRINK_SCALE_STEP;
    scale.vx            = ONE;
    scale.vy            = (s16)work->shrinkScaleY;
    scale.vz            = ONE;
    root->coord         = work->savedRootMtx;
    gfxSetRotIdentity(&shrinkMatrix);
    ScaleMatrix(&shrinkMatrix, &scale);
    MulMatrix(&root->coord, &shrinkMatrix);
    if ((s16)++work->stateFrames == MAD_CHASER_DEATH_BURN_START_FRAME) {
        originOffset.vx = 0;
        originOffset.vy = 0;
        originOffset.vz = 0;
        effectSpawn(EFFECT_CORPSE_BURN, root, MAD_CHASER_DEATH_BURN_BURSTS, &originOffset);
    }
    if ((s16)work->stateFrames == MAD_CHASER_DEATH_BLACK_FRAME) {
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if ((s16)work->stateFrames > MAD_CHASER_DEATH_HIDE_AFTER_FRAME) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
}

/// Records the horizontal range and relative bearing of the nearer player actor.
///
/// Requires live Mad Chaser work/model and any present player models in the
/// same root-parent frame. Always snapshots the full-width root XYZ for collision
/// rollback. If the primary player exists, narrows each candidate XYZ offset to
/// s16 game units before measuring X/Z distance; ties keep the primary player.
/// Stores that offset and distance (narrowed to s16) and a 0..4095 bearing
/// relative to the work heading. Uses SDK normalization/GTE state; horizontal
/// squared sums must fit signed 32 bits. An absent primary leaves prior tracking
/// values intact even when the companion exists. Does not turn or move the model.
static void _madChaserTrackPlayer(Task* task)
{
    /// Saves signed-halfword XYZ and narrows a horizontal distance to s16.
    ///
    /// workBlock is a live MadChaserWork pointer, offsetVector is an SVECTOR
    /// lvalue, and horizontalDistance is in game units. The first two arguments
    /// are evaluated repeatedly and must be stable, disjoint and free of side
    /// effects; the distance is evaluated once. Captures no caller identifiers.
    /// Confined to this function, before normalization overwrites the offset.
#define MAD_CHASER_SAVE_PLAYER_TRACKING(workBlock, offsetVector, horizontalDistance) \
    do {                                                                             \
        (workBlock)->toPlayer.vx = (offsetVector).vx;                                \
        (workBlock)->toPlayer.vy = (offsetVector).vy;                                \
        (workBlock)->toPlayer.vz = (offsetVector).vz;                                \
        (workBlock)->playerDist  = (horizontalDistance);                             \
    } while (0)

    MadChaserWork* work;
    GfxCoord*      root;
    GfxCoord*      playerRoot;
    Task*          playerTask;
    SVECTOR        nearestOffset;
    SVECTOR        companionOffset;
    s32            nearestDistance;
    s32            companionDistance;

    work                 = task->work;
    root                 = task->extra.tmd->coords;
    playerTask           = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = root->coord.t[0];
    work->prevRootPos.vy = root->coord.t[1];
    work->prevRootPos.vz = root->coord.t[2];
    if (playerTask != NULL) {
        playerRoot       = playerTask->extra.tmd->coords;
        nearestOffset.vx = playerRoot->coord.t[0] - root->coord.t[0];
        nearestOffset.vy = playerRoot->coord.t[1] - root->coord.t[1];
        nearestOffset.vz = playerRoot->coord.t[2] - root->coord.t[2];
        nearestDistance  = SquareRoot0(nearestOffset.vx * nearestOffset.vx + nearestOffset.vz * nearestOffset.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
            playerRoot         = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            companionOffset.vx = playerRoot->coord.t[0] - root->coord.t[0];
            companionOffset.vy = playerRoot->coord.t[1] - root->coord.t[1];
            companionOffset.vz = playerRoot->coord.t[2] - root->coord.t[2];
            companionDistance  = SquareRoot0(companionOffset.vx * companionOffset.vx + companionOffset.vz * companionOffset.vz);
            if (companionDistance < nearestDistance) {
                nearestDistance  = companionDistance;
                nearestOffset.vx = companionOffset.vx;
                nearestOffset.vy = companionOffset.vy;
                nearestOffset.vz = companionOffset.vz;
            }
        }
        // Save the displacement before normalization changes it to a direction.
        MAD_CHASER_SAVE_PLAYER_TRACKING(work, nearestOffset, nearestDistance);
        VectorNormalSS(&nearestOffset, &nearestOffset);
        work->playerBearing = (ratan2(nearestOffset.vx, nearestOffset.vz) - work->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
#undef MAD_CHASER_SAVE_PLAYER_TRACKING
}
