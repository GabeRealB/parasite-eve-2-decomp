/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `WorldCollisionContact` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into
/// `gGolemPawnRookShotSound`.
void golemPawnRookBulletSpawn(Enemy* arg0, Task* arg1)
{
    GolemPawnRookFxWork*       work;
    GolemPawnRookPlaceScratch* scratch;
    Enemy*                     ctx;
    GfxCoord*                  coord;
    GfxCoord*                  parentCoord;
    TmdObject*                 tmd;
    Task*                      parent;
    s32                        sound;
    s32                        pan;

    tmd         = arg1->extra.tmd;
    coord       = tmd->coords;
    parent      = arg1->parent;
    parentCoord = parent->extra.tmd->coords;
    work        = memCalloc(0xF0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work    = work;
    tmd->flags    = 0;
    scratch       = (GolemPawnRookPlaceScratch*)SCRATCH_STACK_RESERVE_BYTES(0x38);
    tmd->lightMtx = &work->lightMtx;
    tmd->colorMtx = &work->colorMtx;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = 0;
    scratch->rot.vy = 0x1F4;
    scratch->rot.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    coord->parent      = &gGfxViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = 0x80;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0x10;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);

    work->field_EE = (gGolemPawnRookAttacks[3].reaction != 1);

    work->obj40.coord            = coord;
    work->obj40.context.contacts = work->rec60;
    work->obj40.pos.vx           = 0;
    work->obj40.pos.vy           = 0;
    work->obj40.pos.vz           = 0;
    work->obj40.key              = Gp_PackPair(gGolemPawnRookAttacks, 3);
    work->obj40.radius           = 0x64;
    work->obj40.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(work->rec60, 1, 0);
    work->obj40.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->obj78.coord            = coord;
    work->obj78.context.contacts = work->rec60;
    work->obj78.pos.vx           = 0;
    work->obj78.pos.vy           = 0;
    work->obj78.pos.vz           = 0;
    work->obj78.key              = 0x22B2B;
    work->obj78.radius           = 0x64;
    work->obj78.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->obj78);
    work->obj78.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->d4rec.ends[0].vx      = 0;
    work->d4rec.ends[0].vy      = 0;
    work->d4rec.ends[0].vz      = 0;
    work->d4rec.ends[1].vx      = 0;
    work->d4rec.ends[1].vy      = -0x1F4;
    work->d4rec.ends[1].vz      = 0;
    work->d4rec.end0Radius      = 1;
    work->d4rec.end1Radius      = 1;
    work->d4rec.contacts        = work->recD0;
    work->obj98.context.capsule = &work->d4rec;
    work->obj98.coord           = coord;
    work->obj98.pos.vx          = 0;
    work->obj98.pos.vy          = 0;
    work->obj98.pos.vz          = 0;
    work->obj98.key             = 0;
    work->obj98.radius          = 0;
    work->obj98.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->obj98);
    Gp_InitRec18Table(work->recD0, 1, 0);
    work->obj98.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);

    arg1->state = 1;
    Task_DetachFromParent(arg1);

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);

    ctx   = arg1->spawnArg2.pointer;
    sound = gGolemPawnRookShotSound | (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan   = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));

    SCRATCH_STACK_RELEASE_BYTES(0x38);
}
