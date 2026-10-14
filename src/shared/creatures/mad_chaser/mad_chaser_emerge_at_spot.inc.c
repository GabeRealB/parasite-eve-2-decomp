/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Places the emerging root at a room spot, facing opposite its entry heading.
///
/// Requires live work/root and a loaded spot table. Command bits 8..11 select
/// an existing row; the nibble mask alone does not bound a shorter table. XYZ
/// are parent-coordinate units and heading is wrapped to 4096 units per turn.
/// Borrows all storage and retains the command. Clears pitch/roll and writes
/// root translation; the caller dirties and composes the changed root.
static __inline__ void _madChaserPlaceEmergeRoot(MadChaserWork* work, GfxCoord* root, const OverlayEncounterSpot* spots)
{
    enum {
        MAD_CHASER_EMERGE_SPOT_SHIFT = 8,
        MAD_CHASER_EMERGE_SPOT_MASK  = 0xF
    };

    work->rotation.vx = 0;
    work->rotation.vy = (spots[(work->command >> MAD_CHASER_EMERGE_SPOT_SHIFT) & MAD_CHASER_EMERGE_SPOT_MASK].heading + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
    work->rotation.vz = 0;
    root->coord.t[0]  = spots[(work->command >> MAD_CHASER_EMERGE_SPOT_SHIFT) & MAD_CHASER_EMERGE_SPOT_MASK].x;
    root->coord.t[1]  = spots[(work->command >> MAD_CHASER_EMERGE_SPOT_SHIFT) & MAD_CHASER_EMERGE_SPOT_MASK].y;
    root->coord.t[2]  = spots[(work->command >> MAD_CHASER_EMERGE_SPOT_SHIFT) & MAD_CHASER_EMERGE_SPOT_MASK].z;
}

/// Consumes an emerge command and launches the selected room-entry movement.
///
/// Requires live hidden-form work, enemy and model in emerge behavior zero.
/// Command bits 8..11 must index the loaded room's spots (0..11 in Dumping Hole,
/// 0..15 in Garbage Incinerator); other rooms retain the existing transform.
/// Bits 4..7 select backflip (0), low backward arc (1), or high arc (all others).
/// Enables pair collision and drawing, disables grid tests and limb shadows,
/// resets clip 7 at normal rate and launches at +100 Y units per update. Spawn
/// kind 2 retains its existing primitive buffer; other kinds allocate one.
/// Clears the command after composing the root. The Dumping Hole sound samples
/// the previously composed origin before that composition; audio projection
/// scratch/GTE setup is required. All task-owned storage remains live.
static void _madChaserEmergeAtSpot(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_SPAWN_KIND_MASK      = 0xF,
        MAD_CHASER_EMERGE_RETAIN_BUFFER_KIND   = 2,
        MAD_CHASER_EMERGE_MOVE_SHIFT           = 4,
        MAD_CHASER_EMERGE_MOVE_MASK            = 0xF,
        MAD_CHASER_EMERGE_MOVE_BACKFLIP        = 0,
        MAD_CHASER_EMERGE_MOVE_ARC_BACK        = 1,
        MAD_CHASER_EMERGE_INITIAL_CLIP         = 7,
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT = 8,
        MAD_CHASER_EMERGE_DUMPING_HOLE_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 6)
    };
    MadChaserWork* work  = task->work;
    TmdObject*     model = task->extra.tmd;
    Enemy*         enemy = task->spawnArg2.pointer;
    GfxCoord*      root  = model->coords;
    MadChaserWork* requestWork;
    Enemy*         soundEnemy;
    s32            soundId;
    s32            audioPan;
    u32            stageAreaKey;

    if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_EMERGE) {
        // Reveal the body while deferring grid collision until entry is complete.
        work->shadowHidden    = 1;
        work->pairBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        model->flags         &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if ((task->spawnArg1.value & MAD_CHASER_EMERGE_SPAWN_KIND_MASK) != MAD_CHASER_EMERGE_RETAIN_BUFFER_KIND) {
            tmdAllocPrimitiveBuffer(model);
            model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
        enemy->node.state.parts.flags = 0;
        stageAreaKey                  = GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 0, 0)) {
            _madChaserPlaceEmergeRoot(work, root, gShelterB3DumpingHoleEncounterSpots);
            soundEnemy = task->spawnArg2.pointer;
            soundId    = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_DUMPING_HOLE_SOUND;
            audioPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        } else if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0, 0)) {
            _madChaserPlaceEmergeRoot(work, root, gShelterB3GarbageIncineratorEncounterSpots);
        }
        work->moveAccel          = 0;
        work->moveSpeed          = 100;
        requestWork              = task->work;
        requestWork->animRate    = ANIMATION_RATE_ONE;
        requestWork->animId      = MAD_CHASER_EMERGE_INITIAL_CLIP;
        requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        // The following frame runs the selected entry path with its own counter.
        switch ((work->command >> MAD_CHASER_EMERGE_MOVE_SHIFT) & MAD_CHASER_EMERGE_MOVE_MASK) {
            case MAD_CHASER_EMERGE_MOVE_BACKFLIP:
                _madChaserSetBehaviorStateS16(task, MAD_CHASER_EMERGE_STATE_BACKFLIP);
                break;
            case MAD_CHASER_EMERGE_MOVE_ARC_BACK:
                _madChaserSetBehaviorStateS16(task, MAD_CHASER_EMERGE_STATE_ARC_BACK);
                break;
            default:
                _madChaserSetBehaviorStateS16(task, MAD_CHASER_EMERGE_STATE_HIGH_ARC);
                break;
        }
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(root);
        work->command = 0;
    }
}

/// Levels the landed backflip, reverses heading and launches the forward hop.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage at the Y > 0 crossing of emerge behavior 1. Restores limb
/// shadows, clamps parent Y to -60, levels pitch/roll and adds a half turn to
/// the s16 heading. Resets clip 17 at normal rate and launches at -110 Y units
/// per update with zero acceleration. Clears the u16 frame counter and enters
/// behavior 2; the caller applies animation and rebuilds rotation afterwards.
static __inline__ void _madChaserEmergeBackflipLaunchHop(Task* task, MadChaserWork* work, GfxCoord* root)
{
    enum {
        MAD_CHASER_EMERGE_BACKFLIP_HOP_CLIP    = 17,
        MAD_CHASER_EMERGE_LANDING_Y            = -60,
        MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN   = ACTOR_TRANSFORM_ANGLE_TURN / 2,
        MAD_CHASER_EMERGE_BACKFLIP_HOP_Y_SPEED = -110
    };
    MadChaserWork* requestWork;

    work->shadowHidden       = 0;
    work->stateFrames        = 0;
    root->coord.t[1]         = MAD_CHASER_EMERGE_LANDING_Y;
    work->rotation.vx        = 0;
    work->rotation.vz        = 0;
    work->rotation.vy       += MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_EMERGE_BACKFLIP_HOP_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->moveAccel          = 0;
    work->moveSpeed          = MAD_CHASER_EMERGE_BACKFLIP_HOP_Y_SPEED;
    work->state++;
}

/// Finishes the backward entry flip and launches a forward recovery hop.
///
/// Requires live work/model storage in emerge behavior 1. Moves backward 140
/// parent-coordinate units per updating frame while easing pitch toward a half
/// turn; vertical acceleration grows by two each call. Crossing Y > 0 restores
/// shadows, clears the counter, places Y at -60, levels pitch/roll and adds a
/// half turn to heading. Resets clip 17 at normal rate, launches at -110 Y units
/// per frame and enters behavior 2. Motion and rotation narrow to s16; the caller
/// ticks animation, rebuilds rotation and resolves collision.
static void _madChaserEmergeBackflip(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN      = ACTOR_TRANSFORM_ANGLE_TURN / 2,
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16,
    };
    MadChaserWork* work;
    s16            moveHeading;
    GfxCoord*      root;
    s32            stepDistance;
    s32            directionXQ16;

    work                                  = task->work;
    moveHeading                           = work->rotation.vy;
    root                                  = task->extra.tmd->coords;
    directionXQ16                         = rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS;
    stepDistance                          = -0x8C;
    task->extra.tmd->coords->coord.t[0]  += (directionXQ16 * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->rotation.vx                    += (MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN - work->rotation.vx) >> 3;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 2;
    work->moveSpeed                      += work->moveAccel;
    // Landing launches the next move rather than ending emergence.
    if (root->coord.t[1] > 0) {
        _madChaserEmergeBackflipLaunchHop(task, work, root);
    }
}

/// Starts the entry-hop cue at the root's already-composed origin.
///
/// Requires a live Enemy/model; tags character-bank entry 9 with placement
/// index 0..15 in sound-id bits 8..15. The cached root-to-view transform supplies
/// pan (-16..15) and attenuation depth (-128..127), both narrowed to s8 before
/// the request. Requires projection scratch/GTE setup; retains no storage.
static __inline__ void _madChaserEmergeHopForwardPlayEntrySound(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_HOP_SOUND            = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 9),
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT = 8
    };
    Enemy* enemy = task->spawnArg2.pointer;
    s32    soundId;
    s32    audioPan;

    soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_HOP_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Lands the forward recovery hop after the entry backflip.
///
/// Requires live work, enemy/model storage in emerge behavior 2 with a cleared
/// frame counter. Starts positional character-bank entry 9 on signed frame 1,
/// then moves forward 80 parent-coordinate units each updating frame. Vertical
/// acceleration grows by four; speed/acceleration narrow to s16. Crossing Y > 0
/// places Y at -60, clears the counter and enters creep behavior 3. Audio reads
/// the root's already-composed origin before movement and requires the origin
/// projection scratch/GTE setup. Playback, rotation and collision belong to
/// the caller; all task-owned storage remains live.
static void _madChaserEmergeHopForward(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16,
        MAD_CHASER_EMERGE_LANDING_Y               = -60
    };
    MadChaserWork* work;
    GfxCoord*      root;
    s16            moveHeading;
    s16            stepDistance;

    work = task->work;
    root = task->extra.tmd->coords;
    if ((s16)++work->stateFrames == 1) {
        _madChaserEmergeHopForwardPlayEntrySound(task);
    }
    stepDistance                          = 0x50;
    moveHeading                           = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 4;
    work->moveSpeed                      += work->moveAccel;
    if (root->coord.t[1] > 0) {
        root->coord.t[1]  = MAD_CHASER_EMERGE_LANDING_Y;
        work->stateFrames = 0;
        work->state++;
    }
}
