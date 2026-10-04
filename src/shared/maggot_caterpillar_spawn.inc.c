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
    obj->lightMtx       = &work->field_1F4;
    obj->colorMtx       = &work->field_1D4;
    matrix              = &coord->coord;
    work->field_3C0     = MAGGOT_CATERPILLAR_IS_CATERPILLAR;
    work->field_36C     = &gMaggotCaterpillarBodyTask;
    ctx->field_4        = matrix;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = actor->extra.tmd->coords + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->field_2B4;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &gMaggotCaterpillarParams;
    ctx->hp                    = (s16)gMaggotCaterpillarParams.hpMax;
    work->field_354.coord      = coord;
    work->field_354.spawnArgLo = 0x100;
    work->field_354.spawnArgHi = 1;
    work->field_3C4            = (s16)ctx->place->variant;
    params                     = ctx->place;
    mode                       = params->mode;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->field_392 = 0xC;
                work->field_39A = 0;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 1:
                work->field_39A = mode;
                work->field_392 = mode;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 2:
                work->field_39A    = mode;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 0;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
            case 3:
                work->field_39A    = 2;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 1;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
        }
    } else {
        work->field_3C4 = (s16)params->variant;
        quotient        = mode / 10;
        variant         = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN) == 1) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = gMaggotCaterpillarLeapInYaws[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = gMaggotCaterpillarLeapInSpots;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                } else {
                    work->field_3C2 = 0;
                    work->field_39A = 8;
                    work->field_392 = 1;
                    work->field_3A8 = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == 2) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = gMaggotCaterpillarDropInYaws[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = gMaggotCaterpillarDropInSpots;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                    break;
                }
                work->field_39A    = 8;
                work->field_3C2    = variant;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C8    = variant;
                coord->coord.t[1] += 0x3E8;
        }
    }
    animationInitContext(&work->rig.anim, gMaggotCaterpillarAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 8; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    rec0                             = work->field_234;
    work->field_214.coord            = coord;
    work->field_214.context.contacts = rec0;
    work->field_214.pos.vx           = 0;
    work->field_214.pos.vy           = -0x12C;
    work->field_214.pos.vz           = 0;
    work->field_214.key              = 0x30000 | MAGGOT_CATERPILLAR_ID;
    work->field_214.radius           = 0x12C;
    work->field_214.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_214);
    Gp_InitRec18Table(rec0, 4, 0);
    work->field_214.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->field_294.coord            = actor->extra.tmd->coords + 1;
    rec1                             = work->field_2B4;
    work->field_294.context.contacts = rec1;
    work->field_294.pos.vx           = 0;
    work->field_294.pos.vy           = -0x64;
    work->field_294.pos.vz           = 0;
    work->field_294.key              = 0x30000 | MAGGOT_CATERPILLAR_ID;
    work->field_294.radius           = 0x12C;
    work->field_294.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_294);
    Gp_InitRec18Table(rec1, 2, 0);
    work->field_294.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->field_2E4.coord            = actor->extra.tmd->coords + 4;
    rec2                             = work->field_304;
    work->field_2E4.context.contacts = rec2;
    work->field_2E4.pos.vx           = 0;
    work->field_2E4.pos.vy           = 0;
    work->field_2E4.pos.vz           = 0;
    work->field_2E4.key              = 0;
    work->field_2E4.radius           = 0xC8;
    work->field_2E4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->field_2E4);
    Gp_InitRec18Table(rec2, 1, 0);
    work->field_2E4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->field_31C.coord            = actor->extra.tmd->coords + 4;
    rec3                             = work->field_33C;
    work->field_31C.context.contacts = rec3;
    work->field_31C.pos.vx           = 0;
    work->field_31C.pos.vy           = 0;
    work->field_31C.pos.vz           = 0;
    work->field_31C.key              = 0x22424;
    work->field_31C.radius           = 0x1F4;
    work->field_31C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->field_31C);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_31C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state           = 1;
}
