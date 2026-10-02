/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Spawn handler. Allocates the 0x71C-byte work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `spawnState`. State 0 is the full setup: it links the
/// enemy node, picks the box table and count for the current stage / room out
/// of `gGolemKnightBishopSpots`, requests the room's cue bank, and links the
/// work block's five collision objects with their `WorldCollisionContact` tables before
/// moving the task on (`field_30` 1). States 1 and 2 only seed the animation
/// and sequence state.
void golemKnightBishopSpawn(Enemy* arg0, Task* arg1)
{
    u8                     param1[4];
    u8                     param2[4];
    GolemKnightBishopWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    s16*                   cues;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    WorldCollisionContact* records4;
    WorldCollisionContact* records5;
    s32                    i;
    s32                    kind;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x71C, 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work                 = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_65C.coord      = &arg1->extra.tmd->coords[3];
    work->field_65C.spawnArgLo = 0x500;
    work->field_65C.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, gGolemKnightBishopAnimSets, obj, work->rig.poses, work->rig.slots);
    work->field_6C0 = 0xB;
    work->field_6C2 = 0xB;
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, work->field_6C0);
    }
    kind = arg0->spawnState;
    switch (kind) {
        case 0:
            work->field_6D8         = 0xFF;
            obj->shading.colorBlend = 0;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = -1;
            arg0->field_4   = &coord->coord;
            arg0->field_48  = 0;
            Gp_LinkNode(&arg0->node);
            arg0->coord      = &arg1->extra.tmd->coords[3];
            arg0->bodyPos.vx = 0;
            arg0->bodyPos.vy = 0;
            arg0->bodyPos.vz = 0;
            arg0->param      = &gGolemKnightBishopParams;
            arg0->recs       = work->field_49C;
            arg0->hp         = gGolemKnightBishopParams.hpMax;
            for (i = 0; gGolemKnightBishopSpots[i].field_0 != 0; i++) {
                if (gGameSession->location.loc.stage == gGolemKnightBishopSpots[i].field_2 && gGameSession->location.loc.area == gGolemKnightBishopSpots[i].field_4) {
                    work->field_6B4 = gGolemKnightBishopRegions[gGolemKnightBishopSpots[i].field_0];
                    work->field_6FA = gGolemKnightBishopSpots[i].field_6;
                }
            }
            work->field_6CC = 0xB;
            (Gp_IncStateF0Ref)(0);
            work->field_716 = GOLEM_KNIGHT_BISHOP_ID;
            cues            = gGolemKnightBishopStageCues[gGameSession->location.loc.stage];
            if (cues != NULL) {
                work->field_712 = cues[gGameSession->location.loc.area];
            }
            if (work->field_712 != 0) {
                param1[3] = 0;
                param1[2] = 0x28;
                param1[0] = work->field_712;
                param2[0] = 0x16;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            }
            work->field_484 = &arg1->extra.tmd->coords[3];
            records1        = work->field_49C;
            work->field_488 = records1;
            work->field_48C = 0;
            work->field_48E = 0;
            work->field_490 = 0;
            work->field_494 = 0x30000 | GOLEM_KNIGHT_BISHOP_ID;
            work->field_498 = 0x15E;
            work->field_49A = 1;
            Gp_LinkObj(2, (WorldCollisionBody*)work->field_47C);
            Gp_InitRec18Table(records1, 3, 0);
            work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_4EC  = arg1->extra.tmd->coords;
            records2         = work->field_504;
            work->field_4F0  = records2;
            work->field_4F4  = 0;
            work->field_4F6  = -0x1F4;
            work->field_4F8  = 0;
            work->field_4FC  = 0x30000 | GOLEM_KNIGHT_BISHOP_ID;
            work->field_500  = 0x1F4;
            work->field_502  = 1;
            Gp_LinkObj(2, (WorldCollisionBody*)work->field_4E4);
            Gp_InitRec18Table(records2, 4, 0);
            work->field_502 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_56C  = &arg1->extra.tmd->coords[8];
            records3         = &work->field_584;
            work->field_570  = records3;
            work->field_574  = 0;
            work->field_576  = 0;
            work->field_578  = 0;
            work->field_57C  = Gp_PackPair(gGolemKnightBishopAttacks, 1);
            work->field_580  = 0x12C;
            work->field_582  = 1;
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_564);
            Gp_InitRec18Table(records3, 1, 0);
            work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_5DC  = 0;
            work->field_5DE  = -0x3E8;
            work->field_5E0  = -0x7D0;
            work->field_5E4  = 0;
            work->field_5E6  = -0x3E8;
            work->field_5E8  = 0;
            work->field_5EC  = 0x1F4;
            work->field_5EE  = 0x1F4;
            records4         = &work->field_5F4;
            work->field_5F0  = records4;
            work->field_5A4  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->field_5A8  = &work->field_5DC;
            work->field_5AC  = 0;
            work->field_5AE  = 0;
            work->field_5B0  = 0;
            work->field_5B4  = 0;
            work->field_5B8  = 0;
            work->field_5BA  = 3;
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_59C);
            Gp_InitRec18Table(records4, 1, 0);
            work->field_5BA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_5C4  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->field_5C8  = records4;
            work->field_5CC  = 0;
            work->field_5CE  = -0x320;
            work->field_5D0  = -0x5AA;
            work->field_5D4  = 0;
            work->field_5D8  = 0x1F4;
            work->field_5DA  = 1;
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_5BC);
            work->field_62C  = 0;
            work->field_62E  = -0x514;
            work->field_630  = 0x2710;
            work->field_634  = 0;
            work->field_636  = 0;
            work->field_638  = 0;
            work->field_63C  = 1;
            work->field_63E  = 1;
            records5         = &work->field_644;
            work->field_640  = records5;
            work->field_614  = coord;
            work->field_618  = &work->field_62C;
            work->field_61C  = 0;
            work->field_61E  = 0;
            work->field_620  = 0;
            work->field_624  = 0;
            work->field_628  = 0;
            work->field_62A  = 3;
            work->field_5DA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_60C);
            Gp_InitRec18Table(records5, 1, 0);
            work->field_62A = (work->field_62A & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | 0xC00;
            arg1->msgTable  = gGolemKnightBishopMessages;
            arg1->state     = 1;
            break;
        case 1:
            work->field_6C0         = 0x10;
            work->field_6CE         = 2;
            arg1->state             = 2;
            work->field_6D8         = 0;
            obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
        case 2:
            work->field_6C0         = 0x14;
            work->field_6CE         = kind;
            arg1->state             = kind;
            work->field_6D8         = 0;
            obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
    }
}
