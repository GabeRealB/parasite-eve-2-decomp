/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Places a fresh body block for the actor: allocates its
/// `GolemPawnRookGrenadeWork`, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `WorldCollisionContact` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into
/// `gGolemPawnRookShotSound`.
void golemPawnRookBulletSpawn(Enemy* arg0, Task* arg1)
{
    GolemPawnRookGrenadeWork* work;
    ActorChildPlaceScratch*   scratch;
    Enemy*                    ctx;
    GfxCoord*                 coord;
    GfxCoord*                 parentCoord;
    TmdObject*                tmd;
    Task*                     parent;
    s32                       sound;
    s32                       pan;

    tmd         = arg1->extra.tmd;
    coord       = tmd->coords;
    parent      = arg1->parent;
    parentCoord = parent->extra.tmd->coords;
    work        = memCalloc(sizeof(GolemPawnRookGrenadeWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work    = work;
    tmd->flags    = 0;
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(ActorChildPlaceScratch);
    tmd->lightMtx = &work->lightMtx;
    tmd->colorMtx = &work->colorMtx;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(parentCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->operand.vx = 0;
    scratch->operand.vy = 0x1F4;
    scratch->operand.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->operand);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    coord->parent      = &gGfxViewCoord;
    coord->coord.t[0] += scratch->offset.vx;
    coord->coord.t[1] += scratch->offset.vy;
    coord->coord.t[2] += scratch->offset.vz;

    scratch->operand.vx = 0x80;
    scratch->operand.vy = 0;
    scratch->operand.vz = 0x10;
    RotMatrix(&scratch->operand, &scratch->rotation);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->rotation);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->rotation.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->rotation.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);

    work->burstStyle = (gGolemPawnRookAttacks[3].reaction != 1);

    work->playerStrikeBody.coord            = coord;
    work->playerStrikeBody.context.contacts = work->strikeContacts;
    work->playerStrikeBody.pos.vx           = 0;
    work->playerStrikeBody.pos.vy           = 0;
    work->playerStrikeBody.pos.vz           = 0;
    work->playerStrikeBody.key              = Gp_PackPair(gGolemPawnRookAttacks, 3);
    work->playerStrikeBody.radius           = 0x64;
    work->playerStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->playerStrikeBody);
    worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->enemyStrikeBody.coord            = coord;
    work->enemyStrikeBody.context.contacts = work->strikeContacts;
    work->enemyStrikeBody.pos.vx           = 0;
    work->enemyStrikeBody.pos.vy           = 0;
    work->enemyStrikeBody.pos.vz           = 0;
    work->enemyStrikeBody.key              = 0x22B2B;
    work->enemyStrikeBody.radius           = 0x64;
    work->enemyStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->enemyStrikeBody);
    work->enemyStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->wallCapsule.ends[0].vx   = 0;
    work->wallCapsule.ends[0].vy   = 0;
    work->wallCapsule.ends[0].vz   = 0;
    work->wallCapsule.ends[1].vx   = 0;
    work->wallCapsule.ends[1].vy   = -0x1F4;
    work->wallCapsule.ends[1].vz   = 0;
    work->wallCapsule.end0Radius   = 1;
    work->wallCapsule.end1Radius   = 1;
    work->wallCapsule.contacts     = work->wallContacts;
    work->wallBody.context.capsule = &work->wallCapsule;
    work->wallBody.coord           = coord;
    work->wallBody.pos.vx          = 0;
    work->wallBody.pos.vy          = 0;
    work->wallBody.pos.vz          = 0;
    work->wallBody.key             = 0;
    work->wallBody.radius          = 0;
    work->wallBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->wallBody);
    worldCollisionInitContacts(work->wallContacts, ARRAY_SIZE(work->wallContacts), 0);
    work->wallBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);

    arg1->state = 1;
    taskDetachFromParent(arg1);

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    ctx   = arg1->spawnArg2.pointer;
    sound = gGolemPawnRookShotSound | (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    pan   = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));

    SCRATCH_STACK_RELEASE_BLOCK(ActorChildPlaceScratch);
}
