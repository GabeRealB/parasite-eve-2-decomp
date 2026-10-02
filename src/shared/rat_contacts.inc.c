/* Part of the Rat library; see rat.h. */

/// Per-frame collision pass: applies the pending move to the root coordinate,
/// then walks the three contact records - kind 2 is a hit that damages the
/// enemy, kinds 1 and 3 an obstacle to push out of - and applies the deepest
/// push at the end.
void ratContacts(Task* actor)
{
    RatWork*               work;
    ActorWallPushFrame*    frame;
    Enemy*                 ctx;
    GfxCoord*              coord;
    GfxCoord*              sourceCoord;
    WorldCollisionContact* effectRec;
    WorldCollisionContact* contactRec;
    s32                    push;
    s32                    result;
    s32                    i;
    s32                    depth;
    s32                    x;
    s32                    y;
    s32                    z;
    s32                    boundedDepth;
    s32                    cooldownParam;
    u32                    lastId;
    u32                    id;
    u32                    slot;
    u32                    hitId;
    u32                    damage;

    push   = 0;
    lastId = 0;
    work   = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorWallPushFrame);
    frame  = SCRATCH_STACK_CURSOR(ActorWallPushFrame);
    coord  = actor->extra.tmd->coords;
    ctx    = actor->spawnArg2.pointer;
    result = func_800E0C10((WorldCollisionContact*)&work->field_27C[0x20], &frame->delta, 4, NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += frame->delta.vx.word >> 16;
            coord->coord.t[1] += frame->delta.vy.word >> 16;
            coord->coord.t[2] += frame->delta.vz.word >> 16;
            break;
        case 2:
            coord->coord.t[0] = work->field_360;
            coord->coord.t[1] = work->field_364;
            coord->coord.t[2] = work->field_368;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        if (--work->field_378 <= 0) {
            work->field_378 = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = work->field_22C.contacts.recs[i].key.value;
        switch (id >> 0x10) {
            case 0:
                break;
            case 2:
                if (work->field_378 == 0) {
                    slot                 = id >> 7;
                    sourceCoord          = gPlayerActorTasks[slot & 1]->extra.tmd->coords;
                    frame->delta.vx.word = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vy.word = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vz.word = sourceCoord->coord.t[2] - coord->coord.t[2];
                    damage               = Gp_ComputeDamage(work->field_22C.contacts.recs[i].key.value, SquareRoot0((frame->delta.vx.word * frame->delta.vx.word) + (frame->delta.vy.word * frame->delta.vy.word) + (frame->delta.vz.word * frame->delta.vz.word)), 0, 0);
                    if (Gp_RollEnemyChance(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0) != 0) {
                        damage *= 4;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords, 0, NULL);
                    }
                    func_800DA6E8(&((Enemy*)actor->spawnArg2.pointer)->node, damage, 0);
                    func_800E2C78(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->field_37A = 5;
                        work->field_37C = 0;
                        actor->state    = 2;
                    } else if (work->field_398 == 0) {
                        work->field_37A = 4;
                        work->field_37C = 0;
                    }
                    work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    switch (Gp_GetIdParam0(work->field_22C.contacts.recs[i].key.value) & 0xFFFF) {
                        case 0:
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                        case 8:
                            break;
                        case 2:
                            Gp_SetObjFlag2(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(actor->spawnArg2.pointer, work->field_22C.contacts.recs[i].key.value, 0);
                            break;
                        case 1:
                        case 9:
                            Gp_SetObjFlag1(actor->spawnArg2.pointer);
                            break;
                    }
                    hitId = work->field_22C.contacts.recs[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(hitId) & 0xFFFF, coord, NULL, &work->field_334);
                    }
                    cooldownParam = Gp_GetIdParam2(work->field_22C.contacts.recs[i].key.value);
                    if (cooldownParam > 0) {
                        work->field_378 = cooldownParam;
                    }
                }
                break;
            case 1:
                x                    = coord->workm.t[0] - work->field_22C.contacts.recs[i].point.vx;
                frame->delta.vx.word = x;
                y                    = coord->workm.t[1] - work->field_22C.contacts.recs[i].point.vy;
                frame->delta.vy.word = y;
                z                    = coord->workm.t[2] - work->field_22C.contacts.recs[i].point.vz;
                frame->delta.vz.word = z;
                depth                = work->field_22C.contacts.recs[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                boundedDepth         = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal((VECTOR*)&frame->delta, &frame->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &frame->normal, &frame->dir);
                }
                break;
            case 3:
                x                    = coord->workm.t[0] - work->field_22C.contacts.recs[i].point.vx;
                frame->delta.vx.word = x;
                y                    = coord->workm.t[1] - work->field_22C.contacts.recs[i].point.vy;
                frame->delta.vy.word = y;
                z                    = coord->workm.t[2] - work->field_22C.contacts.recs[i].point.vz;
                frame->delta.vz.word = z;
                depth                = work->field_22C.contacts.recs[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                boundedDepth         = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal((VECTOR*)&frame->delta, &frame->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &frame->normal, &frame->dir);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (push * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (push * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_22C.contacts.recs);
    effectRec = work->attackContacts;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_31A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(effectRec);
    }
    contactRec = work->sensorContacts;
    if (Gp_CountRec18Hi(contactRec, 0x10000) != 0) {
        sourceCoord      = gPlayerActorTasks[(u8)work->sensorContacts[0].key.parts.id >> 7]->extra.tmd->coords;
        work->field_394  = 1;
        work->field_1FA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_33C  = sourceCoord;
    }
    Gp_ClearRec18Occupied(contactRec);
    SCRATCH_STACK_RELEASE_BLOCK(ActorWallPushFrame);
}
