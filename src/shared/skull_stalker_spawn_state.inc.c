#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Creates the animation, lighting and collision state of a hidden Skull Stalker.
///
/// Entry requires a live enemy and a three-part TMD task at the spawn state.
/// Allocation failure destroys both; success gives the task ownership of a
/// zeroed work block, links its target and three collision bodies, acquires a
/// battle reference and advances to active state with a hidden idle animation.
/// Placement mode selects the sound variant; mode 1 on a TMD body also advances
/// the texture page and CLUT row and rebuilds both existing buffer halves.
/// The carrier's animation table must provide entries 0..2 (0 unused).
static void _skullStalkerSpawnState(Enemy* enemy, Task* task)
{
    SkullStalkerWork* work;
    enum { SKULL_STALKER_BODY_CONTACT_KEY = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x2F };
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              targetCoord;
    u32                    nextRandomState;
    WorldCollisionContact* frontSenseContacts;
    WorldCollisionContact* senseContacts;
    WorldCollisionContact* bodyContacts;
    s32                    slotIndex;

    model       = task->extra.tmd;
    rootCoord   = model->coords;
    targetCoord = &rootCoord[1];
    work        = memCalloc(sizeof(SkullStalkerWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord[1].coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = targetCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &gSkullStalkerParams;
    enemy->recs                   = work->bodyContacts;
    enemy->hp                     = gSkullStalkerParams.hpMax;
    // Bind animation and start fully hidden before collision can sense the player.
    animationInitContext(&work->anim, (AnimationSet**)gSkullStalkerAnimSets, model, work->poses, work->slots);
    slotIndex = 1;
    do {
        animationResetSlot(&work->anim, slotIndex, SKULL_STALKER_ANIM_IDLE);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    (sceneAcquireBattleRef)(0);
    work->animId                  = SKULL_STALKER_ANIM_IDLE;
    work->appliedAnim             = SKULL_STALKER_ANIM_IDLE;
    work->hiding                  = 1;
    work->fadeFrames              = SKULL_STALKER_FADE_FRAMES;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    nextRandomState               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->fadeWaitFrames          = ((nextRandomState >> 16) & SKULL_STALKER_HIDDEN_WAIT_JITTER_MASK) + SKULL_STALKER_HIDDEN_WAIT_BASE;
    gRandomLcgState               = nextRandomState;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    // The forward capsule and root sphere sense the player; the small body receives hits.
    work->frontSenseCapsule.ends[0].vz   = 5000;
    work->frontSenseCapsule.end0Radius   = 4000;
    work->frontSenseCapsule.end1Radius   = 2000;
    frontSenseContacts                   = work->frontSenseContacts;
    work->frontSenseCapsule.contacts     = frontSenseContacts;
    work->frontSenseBody.context.capsule = &work->frontSenseCapsule;
    work->frontSenseBody.coord           = rootCoord;
    work->frontSenseBody.pos.vx          = 0;
    work->frontSenseBody.pos.vy          = 0;
    work->frontSenseBody.pos.vz          = 0;
    work->frontSenseBody.key             = 0;
    work->frontSenseBody.radius          = 0;
    work->frontSenseBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->frontSenseBody);
    worldCollisionInitContacts(frontSenseContacts, ARRAY_SIZE(work->frontSenseContacts), 0);
    work->senseBody.coord            = rootCoord;
    senseContacts                    = work->senseContacts;
    work->senseBody.context.contacts = senseContacts;
    work->senseBody.pos.vx           = 0;
    work->senseBody.pos.vy           = 0;
    work->senseBody.pos.vz           = 0;
    work->senseBody.key              = 0;
    work->senseBody.radius           = 2000;
    work->senseBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->frontSenseBody.flags       = work->frontSenseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(senseContacts, ARRAY_SIZE(work->senseContacts), 0);
    bodyContacts                = work->bodyContacts;
    work->body.coord            = rootCoord;
    work->body.context.contacts = bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -200;
    work->body.pos.vz           = 0;
    work->body.key              = SKULL_STALKER_BODY_CONTACT_KEY;
    work->body.radius           = 200;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags       = work->senseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags = work->body.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->variant    = enemy->place->mode;
    if (work->variant == 1 && task->bodyKind == work->variant) {
        model->texturePageOffset++;
        model->clutRowOffset++;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    task->exitCallback = _skullStalkerExit;
    task->state++;
}
