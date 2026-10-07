/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

enum {
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Y     = 500,
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Z     = 100,
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_PITCH = 128,
    GOLEM_PAWN_ROOK_GRENADE_LAUNCH_ROLL  = 16,
};

/// Places a grenade's launch offset and Euler turn in view-coordinate space.
///
/// The roots are live coordinates and placement is call-owned scratch. This
/// composes both parents before building the relative matrix; its GTE column
/// products keep the root translation while applying the launch rotation.
static inline void _golemPawnRookPlaceGrenade(GfxCoord* root, GfxCoord* launcherRoot,
                                              ActorChildPlaceScratch* placement)
{
    // Place the launch offset and Euler turn relative to the view coordinate.
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    launcherRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(launcherRoot);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &launcherRoot->workm, &root->coord);

    placement->operand.vx = 0;
    placement->operand.vy = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Y;
    placement->operand.vz = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_Z;
    gte_SetRotMatrix(&root->coord);
    gte_ldv0(&placement->operand);
    gte_rtv0();
    gte_stlvnl(&placement->offset);
    root->parent      = &gGfxViewCoord;
    root->coord.t[0] += placement->offset.vx;
    root->coord.t[1] += placement->offset.vy;
    root->coord.t[2] += placement->offset.vz;

    placement->operand.vx = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_PITCH;
    placement->operand.vy = 0;
    placement->operand.vz = GOLEM_PAWN_ROOK_GRENADE_LAUNCH_ROLL;
    RotMatrix(&placement->operand, &placement->rotation);
    gte_SetRotMatrix(&root->coord);
    gte_ldclmv(&placement->rotation);
    gte_rtir();
    gte_stclmv(&root->coord);
    gte_ldclmv(&placement->rotation.m[0][1]);
    gte_rtir();
    gte_stclmv(&root->coord.m[0][1]);
    gte_ldclmv(&placement->rotation.m[0][2]);
    gte_rtir();
    gte_stclmv(&root->coord.m[0][2]);
}

/// Launches a grenade from the attached launcher and starts its independent flight.
///
/// `enemy` is the grenade's enemy record and `grenade` is its TMD child task,
/// initially parented to the launcher. Allocates grenade-owned work and lighting
/// matrices, places the root in view-coordinate space, and links player, enemy
/// and wall collision bodies. The grenade detaches before flying; allocation
/// failure destroys it. Placement scratch is released before returning.
static void _golemPawnRookGrenadeSpawn(Enemy* enemy, Task* grenade)
{
    enum {
        GOLEM_PAWN_ROOK_GRENADE_RADIUS            = 100,
        GOLEM_PAWN_ROOK_GRENADE_ENEMY_ATTACK_KEY  = 0x22B2B, // Category 2, weapon row 43 and distance-scale row 43.
        GOLEM_PAWN_ROOK_GRENADE_ATTACK            = 3,
        GOLEM_PAWN_ROOK_GRENADE_REACTION_DARKNESS = 1,
        GOLEM_PAWN_ROOK_GRENADE_WALL_TRAIL_LENGTH = 500,
    };

    GolemPawnRookGrenadeWork* work;
    ActorChildPlaceScratch*   placement;
    Enemy*                    spawnEnemy;
    GfxCoord*                 root;
    GfxCoord*                 launcherRoot;
    TmdObject*                model;
    Task*                     launcher;
    s32                       soundId;
    s32                       audioPan;

    model        = grenade->extra.tmd;
    root         = model->coords;
    launcher     = grenade->parent;
    launcherRoot = launcher->extra.tmd->coords;
    work         = memCalloc(sizeof(GolemPawnRookGrenadeWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, grenade);
        return;
    }
    grenade->work   = work;
    model->flags    = 0;
    placement       = SCRATCH_STACK_RESERVE_BLOCK(ActorChildPlaceScratch);
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;

    _golemPawnRookPlaceGrenade(root, launcherRoot, placement);

    // Two strike spheres share a contact; the trailing capsule tests the room grid.
    work->burstStyle = (gGolemPawnRookAttacks[GOLEM_PAWN_ROOK_GRENADE_ATTACK].reaction != GOLEM_PAWN_ROOK_GRENADE_REACTION_DARKNESS);

    work->playerStrikeBody.coord            = root;
    work->playerStrikeBody.context.contacts = work->strikeContacts;
    work->playerStrikeBody.pos.vx           = 0;
    work->playerStrikeBody.pos.vy           = 0;
    work->playerStrikeBody.pos.vz           = 0;
    work->playerStrikeBody.key              = damagePackAttackKey(gGolemPawnRookAttacks, GOLEM_PAWN_ROOK_GRENADE_ATTACK);
    work->playerStrikeBody.radius           = GOLEM_PAWN_ROOK_GRENADE_RADIUS;
    work->playerStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->playerStrikeBody);
    worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->enemyStrikeBody.coord            = root;
    work->enemyStrikeBody.context.contacts = work->strikeContacts;
    work->enemyStrikeBody.pos.vx           = 0;
    work->enemyStrikeBody.pos.vy           = 0;
    work->enemyStrikeBody.pos.vz           = 0;
    work->enemyStrikeBody.key              = GOLEM_PAWN_ROOK_GRENADE_ENEMY_ATTACK_KEY;
    work->enemyStrikeBody.radius           = GOLEM_PAWN_ROOK_GRENADE_RADIUS;
    work->enemyStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->enemyStrikeBody);
    work->enemyStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->wallCapsule.ends[0].vx   = 0;
    work->wallCapsule.ends[0].vy   = 0;
    work->wallCapsule.ends[0].vz   = 0;
    work->wallCapsule.ends[1].vx   = 0;
    work->wallCapsule.ends[1].vy   = -GOLEM_PAWN_ROOK_GRENADE_WALL_TRAIL_LENGTH;
    work->wallCapsule.ends[1].vz   = 0;
    work->wallCapsule.end0Radius   = 1;
    work->wallCapsule.end1Radius   = 1;
    work->wallCapsule.contacts     = work->wallContacts;
    work->wallBody.context.capsule = &work->wallCapsule;
    work->wallBody.coord           = root;
    work->wallBody.pos.vx          = 0;
    work->wallBody.pos.vy          = 0;
    work->wallBody.pos.vz          = 0;
    work->wallBody.key             = 0;
    work->wallBody.radius          = 0;
    work->wallBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->wallBody);
    worldCollisionInitContacts(work->wallContacts, ARRAY_SIZE(work->wallContacts), 0);
    work->wallBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);

    // The projectile owns its work and no longer follows launcher teardown.
    grenade->state = GOLEM_PAWN_ROOK_TASK_RUNNING;
    taskDetachFromParent(grenade);

    root->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);

    spawnEnemy = grenade->spawnArg2.pointer;
    soundId    = gGolemPawnRookShotSound | ((spawnEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
    audioPan   = (s8)worldCoordGetOriginAudioPan(root);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));

    SCRATCH_STACK_RELEASE_BLOCK(ActorChildPlaceScratch);
}
