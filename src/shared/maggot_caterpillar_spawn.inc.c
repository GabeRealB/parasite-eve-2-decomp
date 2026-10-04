/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Spawn handler: allocates the actor's work block, links the four `WorldCollisionBody`
/// nodes (two collision-record tables, the coordinates and the effect arg) and
/// seeds the initial state from the spawn parameters. The spawn mode splits
/// into a tens digit (unused) and a units digit: 0 and 1 either place the actor
/// on a table row and rotate it to face that row's angle, or fall through to
/// the "walk to the player" state, 2 and 3 lift the coordinate and arm a
/// timer. Modes >= 10 re-read the variant index from the parameters.
void maggotCaterpillarSpawn(Enemy* ctx, Task* actor)
{
    SVECTOR                rot;
    WorldCollisionContact* rec0;
    WorldCollisionContact* rec1;
    WorldCollisionContact* rec2;
    WorldCollisionContact* rec3;
    SVECTOR*               positions;
    MATRIX*                matrix;
    MaggotCaterpillarWork* work;
    s32                    variant;
    s32                    quotient;
    s32                    i;
    s32                    mode;
    AreaPlacement*         params;
    GfxCoord*              coord;
    TmdObject*             obj;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(MaggotCaterpillarWork), 0);
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work         = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    matrix              = &coord->coord;
    work->isCaterpillar = MAGGOT_CATERPILLAR_IS_CATERPILLAR;
    work->taskTable     = &gMaggotCaterpillarBodyTask;
    ctx->field_4        = matrix;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = actor->extra.tmd->coords + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->bodyContacts;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &gMaggotCaterpillarParams;
    ctx->hp                    = (s16)gMaggotCaterpillarParams.hpMax;
    work->effectArg.coord      = coord;
    work->effectArg.spawnArgLo = 0x100;
    work->effectArg.spawnArgHi = 1;
    work->entranceSlot         = ctx->place->variant;
    params                     = ctx->place;
    mode                       = params->mode;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->animId         = MAGGOT_CATERPILLAR_ANIM_DORMANT;
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT;
                work->fallSpeed      = 0x80;
                work->ambushFollower = 0;
                break;
            case 1:
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_AIM;
                work->animId         = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->fallSpeed      = 0x80;
                work->ambushFollower = 0;
                break;
            case 2:
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->animId         = MAGGOT_CATERPILLAR_ANIM_HANG;
                work->fallSpeed      = 0;
                work->ambushFollower = 0;
                work->reactionMode   = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                coord->coord.t[1]   += 0x3E8;
                break;
            case 3:
                work->behaviour      = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->animId         = MAGGOT_CATERPILLAR_ANIM_HANG;
                work->fallSpeed      = 0;
                work->ambushFollower = 1;
                work->reactionMode   = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                coord->coord.t[1]   += 0x3E8;
                break;
        }
    } else {
        work->entranceSlot = params->variant;
        quotient           = mode / 10;
        variant            = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN) == 1) {
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_DORMANT;
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT;
                    work->fallSpeed = 0x80;
                    rot.vx          = 0;
                    rot.vy          = gMaggotCaterpillarLeapInYaws[work->entranceSlot];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = gMaggotCaterpillarLeapInSpots;
                    coord->coord.t[0] = positions[work->entranceSlot].vx;
                    coord->coord.t[1] = positions[work->entranceSlot].vy;
                    coord->coord.t[2] = positions[work->entranceSlot].vz;
                } else {
                    work->entranceKind = 0;
                    work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE;
                    work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                    work->fallSpeed    = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == 2) {
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_DORMANT;
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT;
                    work->fallSpeed = 0x80;
                    rot.vx          = 0;
                    rot.vy          = gMaggotCaterpillarDropInYaws[work->entranceSlot];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = gMaggotCaterpillarDropInSpots;
                    coord->coord.t[0] = positions[work->entranceSlot].vx;
                    coord->coord.t[1] = positions[work->entranceSlot].vy;
                    coord->coord.t[2] = positions[work->entranceSlot].vz;
                    break;
                }
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE;
                work->entranceKind = 1;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_HANG;
                work->fallSpeed    = 0;
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                coord->coord.t[1] += 0x3E8;
        }
    }
    animationInitContext(&work->rig.anim, gMaggotCaterpillarAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 8; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    rec0                            = work->gridContacts;
    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = rec0;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0x12C;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30000 | MAGGOT_CATERPILLAR_ID;
    work->gridBody.radius           = 0x12C;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->gridBody);
    Gp_InitRec18Table(rec0, 4, 0);
    work->gridBody.flags       |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->body.coord            = actor->extra.tmd->coords + 1;
    rec1                        = work->bodyContacts;
    work->body.context.contacts = rec1;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0x64;
    work->body.pos.vz           = 0;
    work->body.key              = 0x30000 | MAGGOT_CATERPILLAR_ID;
    work->body.radius           = 0x12C;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(rec1, 2, 0);
    work->body.flags                 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->attackBody.coord            = actor->extra.tmd->coords + 4;
    rec2                              = work->attackContacts;
    work->attackBody.context.contacts = rec2;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->attackBody.key              = 0;
    work->attackBody.radius           = 0xC8;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->attackBody);
    Gp_InitRec18Table(rec2, 1, 0);
    work->attackBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->flameBody.coord            = actor->extra.tmd->coords + 4;
    rec3                             = work->flameContacts;
    work->flameBody.context.contacts = rec3;
    work->flameBody.pos.vx           = 0;
    work->flameBody.pos.vy           = 0;
    work->flameBody.pos.vz           = 0;
    work->flameBody.key              = 0x22424;
    work->flameBody.radius           = 0x1F4;
    work->flameBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->flameBody);
    Gp_InitRec18Table(rec3, 1, 0);
    work->flameBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state           = 1;
}
