/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Spawn handler of the approach cycle: allocates the 0x6E4-byte work block,
/// binds the animation set and reseeds the nineteen slots, then starts the
/// companion enemy whose model takes its texture page and CLUT row from the
/// current room's area record. `Enemy::spawnState` picks how much of that is
/// kept: 0 also links the list node, the five `Gp_LinkObj` collision nodes with
/// their `WorldCollisionContact` tables and the room's streaming cue, while 1 and 2 only
/// prime the animation state. Entry 0 of `Actor05600_D00098`.
void golemPawnRookSpawn(Enemy* ctx, Task* actor)
{
    GolemPawnRookWork* work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          parts;
    GfxCoord*          partsA;
    GfxCoord*          partsB;
    GfxCoord*          partsC;
    GfxCoord*          partsD;
    GfxCoord*          effParts;
    Enemy*             eff;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                param;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    u32 lcg;
#endif

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = GOLEM_PAWN_ROOK_ID;
    work->field_66C            = gGolemPawnRookTasks;
    work->field_670.coord      = &actor->extra.tmd->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->rig.anim, gGolemPawnRookAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    actorTintModel(Gp_SpawnEnemyFromTable(gGolemPawnRookTasks, 3, 0, ctx)->task->extra.tmd, ctx);
#endif
    eff = Gp_SpawnEnemyFromTable(gGolemPawnRookTasks, 1, 0, ctx);
    actorTintModel(eff->task->extra.tmd, ctx);

    switch (ctx->spawnState) {
        case 0:
            ctx->field_4  = &coord->coord;
            ctx->field_48 = 0;
            Gp_LinkNode(&ctx->node);
            parts           = actor->extra.tmd->coords;
            ctx->bodyPos.vx = 0;
            ctx->bodyPos.vy = 0;
            ctx->bodyPos.vz = 0;
            ctx->param      = gGolemPawnRookParams;
            ctx->recs       = work->field_4EC;
            ctx->coord      = &parts[3];
            ctx->hp         = gGolemPawnRookParams->hpMax;
            Gp_IncStateF0Ref(0);
            work->field_6AC = ctx->place->mode & 1;
            if (work->field_6AC == 0) {
                work->field_694 = 1;
                work->field_6A6 = 0;
            } else {
                work->field_694 = 2;
                work->field_6A6 = 1;
                param           = ctx->place->variant;
                work->field_6DA = param * 1000;
            }

            tbl = gGolemPawnRookAreaParams[gGameSession->location.loc.stage];
            if (tbl != NULL) {
                work->field_6D6 = tbl[gGameSession->location.loc.area];
            }
            if (work->field_6D6 != 0) {
                param1[3] = 0;
                param1[2] = 0xA;
                param1[0] = work->field_6D6;
                param2[0] = GOLEM_PAWN_ROOK_ID;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }

#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
            work->field_6D0 = 0xFA;
#endif
            work->field_49C.ends[0].vz = 0x1F40;
            work->field_49C.end0Radius = 0x3E8;
            work->field_49C.ends[0].vx = 0;
            work->field_49C.ends[0].vy = 0;
            work->field_49C.ends[1].vx = 0;
            work->field_49C.ends[1].vy = 0;
            work->field_49C.ends[1].vz = 0;
            work->field_49C.end1Radius = 0x5DC;
            work->field_49C.contacts   = work->field_4B4;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
            lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_6C4 = ((lcg >> 16) & 1) + 1;
            gRandomLcgState = lcg;
#endif
            partsA                          = actor->extra.tmd->coords;
            work->field_47C.context.capsule = &work->field_49C;
            work->field_47C.pos.vx          = 0;
            work->field_47C.pos.vy          = 0;
            work->field_47C.pos.vz          = 0;
            work->field_47C.key             = 0;
            work->field_47C.radius          = 0;
            work->field_47C.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->field_47C.coord           = &partsA[4];
            Gp_LinkObj(3, &work->field_47C);
            Gp_InitRec18Table(work->field_4B4, 1, 0);
            work->field_47C.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            partsB                           = actor->extra.tmd->coords;
            work->field_4CC.context.contacts = work->field_4EC;
            work->field_4CC.pos.vx           = 0;
            work->field_4CC.pos.vy           = 0;
            work->field_4CC.pos.vz           = 0;
            work->field_4CC.key              = 0x30000 | GOLEM_PAWN_ROOK_ID;
            work->field_4CC.radius           = 0x190;
            work->field_4CC.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->field_4CC.coord            = &partsB[3];
            Gp_LinkObj(2, &work->field_4CC);
            Gp_InitRec18Table(work->field_4EC, 5, 0);
            work->field_4CC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

            partsC                           = actor->extra.tmd->coords;
            work->field_564.pos.vy           = -0x226;
            work->field_564.context.contacts = work->field_584;
            work->field_564.pos.vx           = 0;
            work->field_564.pos.vz           = 0;
            work->field_564.key              = 0;
            work->field_564.radius           = 0x226;
            work->field_564.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->field_564.coord            = partsC;
            Gp_LinkObj(2, &work->field_564);
            Gp_InitRec18Table(work->field_584, 4, 0);
            work->field_564.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            effParts                         = eff->task->extra.tmd->coords;
            work->field_5E4.context.contacts = work->field_604;
            work->field_5E4.pos.vx           = 0;
            work->field_5E4.pos.vy           = 0x1F4;
            work->field_5E4.pos.vz           = 0;
            work->field_5E4.key              = 0;
            work->field_5E4.radius           = 0x1F4;
            work->field_5E4.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->field_5E4.coord            = effParts;
            Gp_LinkObj(3, &work->field_5E4);
            Gp_InitRec18Table(work->field_604, 1, 0);
            work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

            work->field_63C.ends[0].vx      = 0;
            work->field_63C.ends[0].vy      = 0;
            work->field_63C.ends[0].vz      = 0;
            work->field_63C.ends[1].vx      = 0;
            work->field_63C.ends[1].vy      = 0;
            work->field_63C.ends[1].vz      = 0;
            work->field_63C.end0Radius      = 1;
            work->field_63C.end1Radius      = 1;
            work->field_63C.contacts        = work->field_654;
            partsD                          = actor->extra.tmd->coords;
            work->field_61C.context.capsule = &work->field_63C;
            work->field_61C.pos.vx          = 0;
            work->field_61C.pos.vy          = 0;
            work->field_61C.pos.vz          = 0;
            work->field_61C.key             = 0;
            work->field_61C.radius          = 0;
            work->field_61C.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->field_61C.coord           = partsD;
            Gp_LinkObj(3, &work->field_61C);
            Gp_InitRec18Table(work->field_654, 1, 0);
            work->field_61C.flags = (work->field_61C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            actor->state          = 1;
            break;
        case 1:
            work->field_694 = 0x19;
            work->field_6A8 = 2;
            actor->state    = 2;
            break;
        case 2:
            work->field_694 = 0x1D;
            work->field_6A8 = 2;
            actor->state    = 2;
            break;
    }
}
