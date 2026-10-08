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
        stageAreaKey                  = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 0, 0)) {
            _madChaserPlaceEmergeRoot(work, root, D_shelter_b3_dumping_hole_8018B74C);
            soundEnemy = task->spawnArg2.pointer;
            soundId    = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_DUMPING_HOLE_SOUND;
            audioPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        } else if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 0, 0)) {
            _madChaserPlaceEmergeRoot(work, root, D_shelter_b3_garbage_incinerator_801874C4);
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
