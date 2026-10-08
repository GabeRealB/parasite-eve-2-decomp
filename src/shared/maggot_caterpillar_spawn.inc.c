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

    // A standalone statement sequence: do not use under an unbraced conditional.
    // Arguments must be stable; sphere is an lvalue evaluated repeatedly.
    // Its coordinate and contact pointer must already be set to live storage;
    // contactCount covers that table. Y narrows to s16 and radius to u16.
    // Preserve shape stores, list insertion, then contact-table initialization.
#define MAGGOT_CATERPILLAR_LINK_SPHERE(sphere, offsetY, collisionKey, sphereRadius, listIndex, contacts, contactCount) \
    (sphere).pos.vx = 0;                                                                                               \
    (sphere).pos.vy = (offsetY);                                                                                       \
    (sphere).pos.vz = 0;                                                                                               \
    (sphere).key    = (collisionKey);                                                                                  \
    (sphere).radius = (sphereRadius);                                                                                  \
    (sphere).flags  = WORLD_COLLISION_BODY_SPHERE;                                                                     \
    worldCollisionLinkBody((listIndex), &(sphere));                                                                    \
    worldCollisionInitContacts((contacts), (contactCount), 0)

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
    work->taskTable     = &gMaggotCaterpillarBodyTask;
    enemy->field_4      = rootMatrix;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord               = actor->extra.tmd->coords + 1;
    enemy->bodyPos.vy          = MAGGOT_CATERPILLAR_HIT_SPHERE_OFFSET_Y;
    enemy->recs                = work->bodyContacts;
    enemy->bodyPos.vx          = 0;
    enemy->bodyPos.vz          = 0;
    enemy->param               = &gMaggotCaterpillarParams;
    enemy->hp                  = (s16)gMaggotCaterpillarParams.hpMax;
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
                    placementRotation.vy = gMaggotCaterpillarLeapInYaws[work->entranceSlot];
                    placementRotation.vz = 0;
                    RotMatrix(&placementRotation, rootMatrix);
                    entrancePositions = gMaggotCaterpillarLeapInSpots;
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
                    placementRotation.vy = gMaggotCaterpillarDropInYaws[work->entranceSlot];
                    placementRotation.vz = 0;
                    RotMatrix(&placementRotation, rootMatrix);
                    entrancePositions = gMaggotCaterpillarDropInSpots;
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
    animationInitContext(&work->rig.anim, gMaggotCaterpillarAnimSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, MAGGOT_CATERPILLAR_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    gridContacts                    = work->gridContacts;
    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = gridContacts;
    MAGGOT_CATERPILLAR_LINK_SPHERE(work->gridBody, -MAGGOT_CATERPILLAR_GRID_SPHERE_RADIUS, WORLD_COLLISION_CONTACT_ENEMY_BODY | MAGGOT_CATERPILLAR_ID, MAGGOT_CATERPILLAR_GRID_SPHERE_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES, gridContacts, ARRAY_SIZE(work->gridContacts));
    work->gridBody.flags       |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->body.coord            = actor->extra.tmd->coords + 1;
    bodyContacts                = work->bodyContacts;
    work->body.context.contacts = bodyContacts;
    MAGGOT_CATERPILLAR_LINK_SPHERE(work->body, MAGGOT_CATERPILLAR_HIT_SPHERE_OFFSET_Y, WORLD_COLLISION_CONTACT_ENEMY_BODY | MAGGOT_CATERPILLAR_ID, MAGGOT_CATERPILLAR_HIT_SPHERE_RADIUS, WORLD_COLLISION_LIST_ENEMY_BODIES, bodyContacts, ARRAY_SIZE(work->bodyContacts));
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
