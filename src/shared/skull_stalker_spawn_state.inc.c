#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Spawn handler of the second enemy, entry 0 of `Actor04600_D0003C`. It
/// allocates the work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, starts the idle animation fully hidden and
/// rolls the first 0x64..0xA3 frame wait, then links the three bodies with
/// their contact tables. The placement's mode is kept in `variant`; mode 1
/// matching the task's `bodyKind` steps the model's texture page and CLUT
/// row and re-streams it twice. `skullStalkerExit` becomes the exit
/// callback.
void skullStalkerSpawnState(Enemy* arg0, Task* arg1)
{
    SkullStalkerWork*      work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    u32                    seed;
    WorldCollisionContact* frontSenseContacts;
    WorldCollisionContact* senseContacts;
    WorldCollisionContact* bodyContacts;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(sizeof(SkullStalkerWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord[1].coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = part;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &gSkullStalkerParams;
    arg0->recs                   = work->bodyContacts;
    arg0->hp                     = gSkullStalkerParams.hpMax;
    animationInitContext(&work->anim, (AnimationSet**)gSkullStalkerAnimSets, obj, work->poses, work->slots);
    i = 1;
    do {
        animationResetSlot(&work->anim, i, 1);
        i += 1;
    } while (i < ARRAY_SIZE(work->slots));
    (Gp_IncStateF0Ref)(0);
    work->animId                 = SKULL_STALKER_ANIM_IDLE;
    work->appliedAnim            = SKULL_STALKER_ANIM_IDLE;
    work->hiding                 = 1;
    work->fadeFrames             = SKULL_STALKER_FADE_FRAMES;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    seed                         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->fadeWaitFrames         = ((seed >> 16) & 0x3F) + 0x64;
    gRandomLcgState              = seed;
    Gp_SetLightMode(arg1->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    work->frontSenseCapsule.ends[0].vz   = 0x1388;
    work->frontSenseCapsule.end0Radius   = 0xFA0;
    work->frontSenseCapsule.end1Radius   = 0x7D0;
    frontSenseContacts                   = work->frontSenseContacts;
    work->frontSenseCapsule.contacts     = frontSenseContacts;
    work->frontSenseBody.context.capsule = &work->frontSenseCapsule;
    work->frontSenseBody.coord           = coord;
    work->frontSenseBody.pos.vx          = 0;
    work->frontSenseBody.pos.vy          = 0;
    work->frontSenseBody.pos.vz          = 0;
    work->frontSenseBody.key             = 0;
    work->frontSenseBody.radius          = 0;
    work->frontSenseBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->frontSenseBody);
    Gp_InitRec18Table(frontSenseContacts, ARRAY_SIZE(work->frontSenseContacts), 0);
    work->senseBody.coord            = coord;
    senseContacts                    = work->senseContacts;
    work->senseBody.context.contacts = senseContacts;
    work->senseBody.pos.vx           = 0;
    work->senseBody.pos.vy           = 0;
    work->senseBody.pos.vz           = 0;
    work->senseBody.key              = 0;
    work->senseBody.radius           = 0x7D0;
    work->senseBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->frontSenseBody.flags       = work->frontSenseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(3, &work->senseBody);
    Gp_InitRec18Table(senseContacts, ARRAY_SIZE(work->senseContacts), 0);
    bodyContacts                = work->bodyContacts;
    work->body.coord            = coord;
    work->body.context.contacts = bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0xC8;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3002F;
    work->body.radius           = 0xC8;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags       = work->senseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags = work->body.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->variant    = arg0->place->mode;
    if (work->variant == 1 && arg1->bodyKind == work->variant) {
        obj->texturePageOffset++;
        obj->clutRowOffset++;
        if (obj->buffer != NULL) {
            tmdBuildBufferHalf(obj);
            tmdBuildBufferHalf(obj);
        }
    }
    arg1->exitCallback = skullStalkerExit;
    arg1->state++;
}
