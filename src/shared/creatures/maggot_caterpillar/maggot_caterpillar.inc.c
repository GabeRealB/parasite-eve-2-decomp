/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Allocates the enemy's body work, initializes its rig and links four collision spheres.
///
/// Takes the live enemy/task pair with a TMD body and placement record. Modes
/// 0..3 select dormant, aimed, hanging leader or hanging follower behavior;
/// modes >=10 use decimal suffix 0/1 for the scripted leap/drop entrances.
/// After the corresponding room event, those scripted modes instead place
/// a dormant enemy at a saved entrance spot. The variant must be 0..3 for
/// entrance tables, and the placement row 0..7 for later timing tables.
/// Success transfers zeroed primary-heap work to the task and enters ACTIVE;
/// allocation failure destroys the enemy/task pair before returning.
static void _maggotCaterpillarSpawn(Enemy* enemy, Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_PLACE_DORMANT            = 0,
        MAGGOT_CATERPILLAR_PLACE_AIMED              = 1,
        MAGGOT_CATERPILLAR_PLACE_AMBUSH_LEADER      = 2,
        MAGGOT_CATERPILLAR_PLACE_AMBUSH_FOLLOWER    = 3,
        MAGGOT_CATERPILLAR_PLACE_SCRIPTED_BASE      = 10,
        MAGGOT_CATERPILLAR_AMBUSH_START_RISE        = 1000,
        MAGGOT_CATERPILLAR_HIT_EFFECT_SIZE_ARGUMENT = 0x100,
        MAGGOT_CATERPILLAR_GRID_SPHERE_RADIUS       = 300,
        MAGGOT_CATERPILLAR_HIT_SPHERE_RADIUS        = 300,
        MAGGOT_CATERPILLAR_HIT_SPHERE_OFFSET_Y      = -100,
        MAGGOT_CATERPILLAR_ATTACK_SPHERE_RADIUS     = 200,
        MAGGOT_CATERPILLAR_FLAME_SPHERE_RADIUS      = 500
    };
    SVECTOR                placementRotation;
    WorldCollisionContact* gridContacts;
    WorldCollisionContact* bodyContacts;
    WorldCollisionContact* attackContacts;
    WorldCollisionContact* flameContacts;
    SVECTOR*               entrancePositions;
    MATRIX*                rootMatrix;
    MaggotCaterpillarWork* work;
    s32                    entranceDigit;
    s32                    modeTens;
    s32                    slotIndex;
    s32                    placementMode;
    AreaPlacement*         placement;
    GfxCoord*              coord;
    TmdObject*             model;

    /// Sets a local sphere's shape, links it, then initializes its contact table.
    ///
    /// `sphere` is an unlinked writable WorldCollisionBody lvalue, evaluated
    /// seven times; its coord/context.contacts must already borrow live storage.
    /// Other arguments are evaluated once and must be stable and independent of
    /// the destination. `contacts` must be that body's contact table, with
    /// positive `contactCount` measured in elements and covering its full extent.
    /// Centre Y narrows to s16, radius to u16 in local coordinate units; the
    /// packed key stays s32. Leaves pos.pad untouched; replaces flags with
    /// sphere kind before linking to `listIndex`. The caller enables passes
    /// afterwards and keeps the body, transform and contacts alive until unlink.
    /// Captures no locals; expands to one statement.
#define MAGGOT_CATERPILLAR_LINK_SPHERE(sphere, offsetY, collisionKey, sphereRadius, listIndex, contacts, contactCount) \
    (void)({                                                                                                           \
        (sphere).pos.vx = 0;                                                                                           \
        (sphere).pos.vy = (offsetY);                                                                                   \
        (sphere).pos.vz = 0;                                                                                           \
        (sphere).key    = (collisionKey);                                                                              \
        (sphere).radius = (sphereRadius);                                                                              \
        (sphere).flags  = WORLD_COLLISION_BODY_SPHERE;                                                                 \
        worldCollisionLinkBody((listIndex), &(sphere));                                                                \
        worldCollisionInitContacts((contacts), (contactCount), 0);                                                     \
    })

    model = actor->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work         = work;
    model->flags        = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx     = &work->lightMtx;
    model->colorMtx     = &work->colorMtx;
    rootMatrix          = &coord->coord;
    work->isCaterpillar = MAGGOT_CATERPILLAR_IS_CATERPILLAR;
    work->taskTable     = MAGGOT_CATERPILLAR_TASK_TABLE;
    enemy->field_4      = rootMatrix;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord               = actor->extra.tmd->coords + 1;
    enemy->bodyPos.vy          = MAGGOT_CATERPILLAR_HIT_SPHERE_OFFSET_Y;
    enemy->recs                = work->bodyContacts;
    enemy->bodyPos.vx          = 0;
    enemy->bodyPos.vz          = 0;
    enemy->param               = &_gMaggotCaterpillarParams;
    enemy->hp                  = (s16)_gMaggotCaterpillarParams.hpMax;
    work->effectArg.coord      = coord;
    work->effectArg.spawnArgLo = MAGGOT_CATERPILLAR_HIT_EFFECT_SIZE_ARGUMENT;
    work->effectArg.spawnArgHi = 1;
    work->entranceSlot         = enemy->place->variant;
    placement                  = enemy->place;
    placementMode              = placement->mode;
    if (placementMode < MAGGOT_CATERPILLAR_PLACE_SCRIPTED_BASE) {
        switch (placementMode) {
            case MAGGOT_CATERPILLAR_PLACE_DORMANT:
                work->animId         = MAGGOT_CATERPILLAR_ANIM_DORMANT;
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT;
                work->fallSpeed      = MAGGOT_CATERPILLAR_GROUND_FALL_STEP;
                work->ambushFollower = 0;
                break;
            case MAGGOT_CATERPILLAR_PLACE_AIMED:
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_AIM;
                work->animId         = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->fallSpeed      = MAGGOT_CATERPILLAR_GROUND_FALL_STEP;
                work->ambushFollower = 0;
                break;
            case MAGGOT_CATERPILLAR_PLACE_AMBUSH_LEADER:
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->animId         = MAGGOT_CATERPILLAR_ANIM_HANG;
                work->fallSpeed      = 0;
                work->ambushFollower = 0;
                work->reactionMode   = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                coord->coord.t[1]   += MAGGOT_CATERPILLAR_AMBUSH_START_RISE;
                break;
            case MAGGOT_CATERPILLAR_PLACE_AMBUSH_FOLLOWER:
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->animId         = MAGGOT_CATERPILLAR_ANIM_HANG;
                work->fallSpeed      = 0;
                work->ambushFollower = 1;
                work->reactionMode   = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                coord->coord.t[1]   += MAGGOT_CATERPILLAR_AMBUSH_START_RISE;
                break;
        }
    } else {
        work->entranceSlot = placement->variant;
        modeTens           = placementMode / MAGGOT_CATERPILLAR_PLACE_SCRIPTED_BASE;
        entranceDigit      = placementMode - modeTens * MAGGOT_CATERPILLAR_PLACE_SCRIPTED_BASE;
        switch (entranceDigit) {
            case MAGGOT_CATERPILLAR_ENTRANCE_LEAP:
                if (gameFlagGetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN) == 1) {
                    work->animId         = MAGGOT_CATERPILLAR_ANIM_DORMANT;
                    work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT;
                    work->fallSpeed      = MAGGOT_CATERPILLAR_GROUND_FALL_STEP;
                    placementRotation.vx = 0;
                    placementRotation.vy = _gMaggotCaterpillarLeapInYaws[work->entranceSlot];
                    placementRotation.vz = 0;
                    RotMatrix(&placementRotation, rootMatrix);
                    entrancePositions = _gMaggotCaterpillarLeapInSpots;
                    coord->coord.t[0] = entrancePositions[work->entranceSlot].vx;
                    coord->coord.t[1] = entrancePositions[work->entranceSlot].vy;
                    coord->coord.t[2] = entrancePositions[work->entranceSlot].vz;
                } else {
                    work->entranceKind = MAGGOT_CATERPILLAR_ENTRANCE_LEAP;
                    work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE;
                    work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                    work->fallSpeed    = 0;
                }
                break;
            case MAGGOT_CATERPILLAR_ENTRANCE_DROP:
                if (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == 2) {
                    work->animId         = MAGGOT_CATERPILLAR_ANIM_DORMANT;
                    work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT;
                    work->fallSpeed      = MAGGOT_CATERPILLAR_GROUND_FALL_STEP;
                    placementRotation.vx = 0;
                    placementRotation.vy = _gMaggotCaterpillarDropInYaws[work->entranceSlot];
                    placementRotation.vz = 0;
                    RotMatrix(&placementRotation, rootMatrix);
                    entrancePositions = _gMaggotCaterpillarDropInSpots;
                    coord->coord.t[0] = entrancePositions[work->entranceSlot].vx;
                    coord->coord.t[1] = entrancePositions[work->entranceSlot].vy;
                    coord->coord.t[2] = entrancePositions[work->entranceSlot].vz;
                    break;
                }
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE;
                work->entranceKind = MAGGOT_CATERPILLAR_ENTRANCE_DROP;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_HANG;
                work->fallSpeed    = 0;
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                coord->coord.t[1] += MAGGOT_CATERPILLAR_AMBUSH_START_RISE;
        }
    }
    // Seed all driven parts before exposing the initialized collision bodies.
    animationInitContext(&work->rig.anim, _gMaggotCaterpillarAnimSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, MAGGOT_CATERPILLAR_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    gridContacts                    = work->gridContacts;
    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = gridContacts;
    MAGGOT_CATERPILLAR_LINK_SPHERE(work->gridBody, -MAGGOT_CATERPILLAR_GRID_SPHERE_RADIUS, WORLD_COLLISION_CONTACT_ENEMY_BODY | MAGGOT_CATERPILLAR_COLLISION_TYPE_ID, MAGGOT_CATERPILLAR_GRID_SPHERE_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES, gridContacts, ARRAY_SIZE(work->gridContacts));
    work->gridBody.flags       |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->body.coord            = actor->extra.tmd->coords + 1;
    bodyContacts                = work->bodyContacts;
    work->body.context.contacts = bodyContacts;
    MAGGOT_CATERPILLAR_LINK_SPHERE(work->body, MAGGOT_CATERPILLAR_HIT_SPHERE_OFFSET_Y, WORLD_COLLISION_CONTACT_ENEMY_BODY | MAGGOT_CATERPILLAR_COLLISION_TYPE_ID, MAGGOT_CATERPILLAR_HIT_SPHERE_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES, bodyContacts, ARRAY_SIZE(work->bodyContacts));
    work->body.flags                 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->attackBody.coord            = actor->extra.tmd->coords + 4;
    attackContacts                    = work->attackContacts;
    work->attackBody.context.contacts = attackContacts;
    MAGGOT_CATERPILLAR_LINK_SPHERE(work->attackBody, 0, 0, MAGGOT_CATERPILLAR_ATTACK_SPHERE_RADIUS, WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackContacts, ARRAY_SIZE(work->attackContacts));
    work->attackBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->flameBody.coord            = actor->extra.tmd->coords + 4;
    flameContacts                    = work->flameContacts;
    work->flameBody.context.contacts = flameContacts;
    MAGGOT_CATERPILLAR_LINK_SPHERE(work->flameBody, 0, WORLD_COLLISION_CONTACT_ATTACK | (MAGGOT_CATERPILLAR_FLAME_ATTACK_ROW << 8) | MAGGOT_CATERPILLAR_FLAME_ATTACK_ROW, MAGGOT_CATERPILLAR_FLAME_SPHERE_RADIUS, WORLD_COLLISION_LIST_PLAYER_ATTACKS, flameContacts, ARRAY_SIZE(work->flameContacts));
    work->flameBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state           = MAGGOT_CATERPILLAR_TASK_ACTIVE;
#undef MAGGOT_CATERPILLAR_LINK_SPHERE
}

#undef MAGGOT_CATERPILLAR_TASK_TABLE

/// Advances the live enemy frame or redraws its paused presentation.
///
/// Requires successful setup and the owning enemy/task pair. PAUSED only
/// updates lighting and shadow; HIDDEN suppresses drawing and lock-on.
/// RUNNING restores both, while other control values perform the full frame
/// without that reset. Status and contacts precede behavior selection; burning,
/// turning, movement and animation precede root composition and presentation.
static void _maggotCaterpillarTick(Enemy* enemy, Task* actor)
{
    s32                    actorControl;
    TmdObject*             model;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;

    model        = actor->extra.tmd;
    actorControl = gSceneCombatState.actorControl;
    work         = actor->work;
    coord        = model->coords;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _maggotCaterpillarUpdateColor(actor);
            _maggotCaterpillarDrawShadow(actor);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Reactions may change both the behavior and the task's next-frame phase.
    if (enemy->reactionFlags != 0) {
        _maggotCaterpillarApplyStatus(actor);
    }
    _maggotCaterpillarResolveContacts(actor);
    _maggotCaterpillarRunBehaviour(actor);
    if (work->burning != 0) {
        _maggotCaterpillarBurnStep(actor);
    }
    if (work->turnRate != 0) {
        _maggotCaterpillarTurnStep(actor);
    }
    _maggotCaterpillarMoveStep(actor);
    _maggotCaterpillarTickAnim(actor);
    // Publish the final pose before querying lighting and drawing its shadow.
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _maggotCaterpillarUpdateColor(actor);
    _maggotCaterpillarDrawShadow(actor);
}

/// Applies buildup and damage-over-time reactions to the actor.
///
/// Committed actions retain their behavior while HP changes; other actions
/// enter stun, hurt or the dying task phase. Damage is narrowed to a signed
/// halfword for the readout and zero test, and HP is stored as a halfword.
/// Expired damage-over-time flags are cleared after the pulse.
static void _maggotCaterpillarApplyStatus(Task* actor)
{
    MaggotCaterpillarWork* work;
    s32                    damage;
    s32                    remainingHp;
    u8                     reactionFlags;
    Enemy*                 enemy;

    enemy         = actor->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = actor->work;
    if ((reactionFlags & ENEMY_REACTION_BUILDUP) && (work->reactionMode != MAGGOT_CATERPILLAR_REACTION_COMMITTED)) {
        enemy->reactionFlags = (u8)(reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_STUN;
        work->step           = 0;
        work->stunned        = 1;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = damageTickEnemyDamageOverTime(enemy);
        if ((s16)damage != 0) {
            worldTargetAddReadoutAmount(&enemy->node, (s16)damage, 0);
            // Preserve halfword HP arithmetic, including the unsigned input view.
            remainingHp = (u16)enemy->hp - damage;
            enemy->hp   = remainingHp;
            if (work->reactionMode != MAGGOT_CATERPILLAR_REACTION_COMMITTED) {
                if ((s16)remainingHp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                } else {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_HURT;
                    work->step      = 0;
                }
            }
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags = (u8)(enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
}

/// Runs the current behavior after this frame's contact and status reactions.
///
/// Requires initialized enemy work and model state. Handlers update movement,
/// turn and animation requests for the rest of the frame. DEAD and values
/// outside the ten defined behaviors run no handler; task death is dispatched
/// separately through `Task::state`.
static void _maggotCaterpillarRunBehaviour(Task* actor)
{
    MaggotCaterpillarWork* work;

    work = actor->work;
    switch (work->behaviour) {
        case MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT:
            _maggotCaterpillarWaitState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_AIM:
            _maggotCaterpillarAimState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH:
            _maggotCaterpillarAmbushState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM:
            _maggotCaterpillarRoamState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY:
            _maggotCaterpillarSprayState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE:
            _maggotCaterpillarPounceState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_HURT:
            _maggotCaterpillarHurtState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_STUN:
            _maggotCaterpillarStunState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE:
            _maggotCaterpillarEntranceState(actor);
            break;
        case MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD:
            break;
    }
}

#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Holds the actor still until its buildup reaction expires.
///
/// Starts the stun clip once and ticks the enemy buildup timer on subsequent
/// calls. Expiry clears the stun latch and returns to roam with the hurt clip
/// and an idle delay of 0..15 ticks.
static void _maggotCaterpillarStunState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_STUN_START = 0,
        MAGGOT_CATERPILLAR_STUN_WAIT  = 1,
    };
    MaggotCaterpillarWork* work;
    s16                    step;
    u32                    random;

    work = actor->work;
    step = work->step;
    switch (step) {
        case MAGGOT_CATERPILLAR_STUN_START:
            work->animId       = MAGGOT_CATERPILLAR_ANIM_STUN;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->step         = MAGGOT_CATERPILLAR_STUN_WAIT;
            return;
        case MAGGOT_CATERPILLAR_STUN_WAIT:
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_HURT;
                work->stunned      = 0;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = random;
                work->stateCounter = (random >> 0x10) & 0xF;
            }
            return;
    }
}

/// Saves the root translation and applies the current horizontal and falling steps.
///
/// `forwardSpeed` and `fallSpeed` are coordinate units per tick. Horizontal
/// movement uses the root forward-axis coefficients with 12 fractional bits;
/// fall speed is added directly to Y. The caller refreshes the composed matrix
/// after behavior, turning, movement and animation finish.
static void _maggotCaterpillarMoveStep(Task* actor)
{
    GfxCoord*              coord;
    MaggotCaterpillarWork* work;

    coord = actor->extra.tmd->coords;
    work  = actor->work;

    work->prevPos.vx = coord->coord.t[0];
    work->prevPos.vy = coord->coord.t[1];
    work->prevPos.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    coord->coord.t[1] += work->fallSpeed;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
}

/// Restarts changed animation requests or advances the seven driven model parts.
///
/// Requires initialized work, model coordinates and animation slots 1..7,
/// plus a clip and blend-table entry for the requested `animId`. A restart
/// resets the frame counter and blends from record zero; an unchanged request
/// increments the counter and advances each slot once. Slot zero is untouched.
static void _maggotCaterpillarTickAnim(Task* actor)
{
    _maggotCaterpillarTickAnimInline(actor);
}

/// Updates enemy lighting and colour from the root's cached translation.
///
/// Requires a live model, writable lighting matrices and the owning enemy in
/// `spawnArg2.pointer`. Copies signed-word XYZ without composing or converting
/// the root; the lighting query interprets the sample as world coordinates.
/// The sample is temporary and no pointer to it is retained.
static void _maggotCaterpillarUpdateColor(Task* actor)
{
    GfxCoord* coord;
    VECTOR    colorPosition;

    coord            = actor->extra.tmd->coords;
    colorPosition.vx = coord->workm.t[0];
    colorPosition.vy = coord->workm.t[1];
    colorPosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &colorPosition, 0, 0);
}

/// Draws the root's ground shadow, probing the floor during a hanging ambush.
///
/// Requires initialized work and a composed root coordinate. The shadow has
/// a 512-unit half-size and base grayscale 128. During the ambush, the probe's
/// signed view-Y displacement narrows to s16 before the hit test and shade
/// calculation; a zero result draws nothing. Other behaviors use the cached
/// root translation directly, without refreshing its composition.
static void _maggotCaterpillarDrawShadow(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE = 512,
        MAGGOT_CATERPILLAR_SHADOW_SHADE     = 128
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    VECTOR3                shadowPosition;
    s16                    probeDisplacement;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    if (work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH) {
        probeDisplacement = worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&coord->workm), &shadowPosition);
        if (probeDisplacement != 0) {
            effectDrawGroundShadow(&shadowPosition, MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE, effectGetGroundShadowShade(MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE, MAGGOT_CATERPILLAR_SHADOW_SHADE, probeDisplacement));
        }
    } else {
        shadowPosition.vx = coord->workm.t[0];
        shadowPosition.vy = coord->workm.t[1];
        shadowPosition.vz = coord->workm.t[2];
        effectDrawGroundShadow(&shadowPosition, MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE, MAGGOT_CATERPILLAR_SHADOW_SHADE);
    }
}

/// Progressively flattens the corpse root from a saved unscaled local matrix.
///
/// The vertical Q12 scale loses 80 each tick while above 512; the subtraction
/// can finish below 512. X and Z retain unit scale. `baseMatrix` must contain
/// the saved local transform so repeated calls do not compound the scale.
/// Releases its scale scratch and marks the root for recomposition.
static void _maggotCaterpillarSquash(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_SQUASH_MIN_SCALE  = 512,
        MAGGOT_CATERPILLAR_SQUASH_SCALE_STEP = 80,
    };
    GfxCoord*              coord;
    ActorScaleScratch*     scratchEnd;
    ActorScaleScratch*     scratch;
    MaggotCaterpillarWork* work;

    scratchEnd                              = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = actor->work;
    scratch                                 = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = actor->extra.tmd->coords;
    if (work->vertical.squashScale > MAGGOT_CATERPILLAR_SQUASH_MIN_SCALE) {
        work->vertical.squashScale -= MAGGOT_CATERPILLAR_SQUASH_SCALE_STEP;
    }
    /// Rebuilds the corpse root from its saved local basis at the current Q12 Y scale.
    ///
    /// Requires disjoint writable GfxCoord/ActorScaleScratch storage and live
    /// work containing the saved baseMatrix and squashScale. All pointers repeat
    /// and must be stable and side-effect-free. X/Z remain ONE; restoring the
    /// saved matrix prevents scale compounding and retains its translation.
    /// Dirties composition and releases exactly one ActorScaleScratch block
    /// through the captured scratch-stack cursor. Scratch is invalid afterward.
    /// The do/while statement is undefined immediately after its sole call.
#define MAGGOT_CATERPILLAR_REBUILD_CORPSE_SCALE(coord, work, scratch) \
    do {                                                              \
        (scratch)->scale.vx = ONE;                                    \
        (scratch)->scale.vy = (work)->vertical.squashScale;           \
        (scratch)->scale.vz = ONE;                                    \
        (coord)->coord      = (work)->baseMatrix;                     \
        gfxSetRotIdentity(&(scratch)->matrix);                        \
        ScaleMatrix(&(scratch)->matrix, &(scratch)->scale);           \
        MulMatrix(&(coord)->coord, &(scratch)->matrix);               \
        (coord)->composeStamp = GRAPHICS_COORD_DIRTY;                 \
        SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);               \
    } while (0)
    MAGGOT_CATERPILLAR_REBUILD_CORPSE_SCALE(coord, work, scratch);
#undef MAGGOT_CATERPILLAR_REBUILD_CORPSE_SCALE
}

/// Flings a detached husk model from body coordinate 4 with its placement textures.
///
/// Requires a live model with coordinate 4 and an owning enemy whose placement
/// index exists in the current area variant. Publishes the borrowed model
/// source for the bank-4 effect descriptor; the carrier must stay loaded while
/// the effect uses it. Allocation failure leaves no husk. On success, both
/// primitive-buffer halves are rebuilt after applying texture-page and CLUT
/// offsets. The parent actor retains ownership of its own model and work.
static void _maggotCaterpillarSpawnHusk(Task* actor)
{
    enum { MAGGOT_CATERPILLAR_HUSK_SIZE_ARGUMENT = 0x100 };
    GameLocationKey  location;
    GameLocationKey* currentLocation;
    u8               currentView;
    AreaVariant*     areaVariant;
    AreaPlacement*   placement;
    EffectWork*      huskEffect;
    TmdObject*       huskModel;
    s32              placementIndex;
    u32              placeKey;

    gTaskBank4Descs[TASK_BANK4_BURST_MODEL_PART_SLOT].data.model = &_gCrawlerHusk;
    huskEffect                                                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, actor->extra.tmd->coords + 4, MAGGOT_CATERPILLAR_HUSK_SIZE_ARGUMENT, NULL);
    if (huskEffect == NULL) {
        return;
    }
    currentLocation    = &gGameSession->location;
    placeKey           = ((Enemy*)actor->spawnArg2.pointer)->placeKey;
    huskModel          = huskEffect->task->extra.tmd;
    location.stage     = currentLocation->stage;
    location.area      = currentLocation->area;
    location.roomState = currentLocation->roomState;
    currentView        = currentLocation->view;
    placementIndex     = placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view      = currentView;
    areaSyncLocationVariant(&location);
    areaVariant = areaGetVariant(&location);
    // Preserve the table helper's scaled-offset-first address calculation.
    placement                    = AREA_PLACEMENT_AT(areaVariant->placements, placementIndex);
    huskModel->texturePageOffset = placement->texturePageOffset;
    huskModel->clutRowOffset     = placement->clutRowOffset;
    if (huskModel->buffer != NULL) {
        tmdBuildBufferHalf(huskModel);
        tmdBuildBufferHalf(huskModel);
    }
}

/// Shrinks the corpse model third coordinate node uniformly to one sixteenth.
///
/// The task must own a model with at least three coordinates. The scale
/// multiplies its existing local rotation and leaves translation unchanged.
/// One scale scratch block is reserved and released. The caller controls
/// recomposition; this routine leaves the compose stamp intact.
static void _maggotCaterpillarShrinkNode2(Task* actor)
{
    void**             cursorSlot;
    ActorScaleScratch* scratchEnd;
    ActorScaleScratch* scratch;
    GfxCoord*          coords;

    cursorSlot                                             = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd                                             = SCRATCH_STACK_CURSOR_AT(cursorSlot, ActorScaleScratch);
    scratch                                                = scratchEnd - 1;
    SCRATCH_STACK_CURSOR_AT(cursorSlot, ActorScaleScratch) = scratch;
    coords                                                 = actor->extra.tmd->coords;

    scratch->scale.vx = ONE / 16;
    scratch->scale.vy = ONE / 16;
    scratch->scale.vz = ONE / 16;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coords[2].coord, &scratch->matrix);
    SCRATCH_STACK_RELEASE_BLOCK_AT(cursorSlot, ActorScaleScratch);
}

/// Dispatches one puff setup, flight or destruction handler.
///
/// `task->state` must be 0..2 and `spawnArg2.pointer` its live enemy context.
/// The three callbacks are copied by value before dispatch; a handler may
/// destroy the task and its work. The carrier and parent actor remain loaded.
static void _maggotCaterpillarPuffTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gMaggotCaterpillarPuffStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Allocates a puff work block and launches it from its parent actor fifth node.
///
/// The task owns a coordinate body, has a live TMD parent with at least five
/// nodes and uses its own enemy context for teardown. The launch transform is
/// expressed beneath the view coordinate. Its owned collision sphere is linked
/// to the enemy-attack list and borrows the work contact array. Allocation
/// failure destroys the child; success advances to the flying task phase.
static void _maggotCaterpillarPuffSetup(Enemy* enemy, Task* task)
{
    Task*                      parent;
    TmdObject*                 parentModel;
    GfxCoord*                  coord;
    MaggotCaterpillarWork*     parentWork;
    GfxCoord*                  parentCoord;
    MaggotCaterpillarPuffWork* work;
    u16                        sprayOrdinal;

    parent      = task->parent;
    parentModel = parent->extra.tmd;
    coord       = task->extra.coordBody->coord;
    parentWork  = parent->work;
    parentCoord = &parentModel->coords[4];
    work        = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Detach the launch pose into the view frame before linking collision.
    task->work                 = work;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(parentCoord);
    coord->parent = &gGfxViewCoord;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    work->forwardSpeed          = MAGGOT_CATERPILLAR_PUFF_INITIAL_SPEED;
    sprayOrdinal                = parentWork->puffCount;
    work->body.coord            = coord;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.context.contacts = work->contacts;
    work->sprayOrdinal          = sprayOrdinal;
    work->body.key              = damagePackAttackKey(_gMaggotCaterpillarAttacks, MAGGOT_CATERPILLAR_ATTACK_PUFF);
    work->body.radius           = 0x100;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state       = MAGGOT_CATERPILLAR_PUFF_TASK_FLY;
}

/// Dispatches one body setup, active-frame or dying handler.
///
/// `actor->state` must be 0..2 and `spawnArg2.pointer` its live owning enemy.
/// Copies all three callbacks before dispatch. Setup failure or completed
/// death may destroy the task; neither argument is used after dispatch.
static void _maggotCaterpillarTask(Task* actor)
{
    EnemyTaskFuncTable3 states;

    states = _gMaggotCaterpillarStates;
    states.funcs[actor->state](actor->spawnArg2.pointer, actor);
}
