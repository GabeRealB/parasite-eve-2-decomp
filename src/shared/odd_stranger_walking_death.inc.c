/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Writable placement-vector storage for the walking burst-death effects.
///
/// `ODD_STRANGER_HAS_EFFECT_OFFSET` selects the live work member in variant 1
/// or the caller's local SVECTOR lvalue in variant 2. Arguments must be stable
/// and side-effect-free; only the selected argument is evaluated, once per
/// expansion. Captures no locals. XYZ are part-local game-coordinate offsets;
/// the fourth halfword remains untouched. Spawning copies XYZ and retains the
/// address, but these particle/debris callbacks never dereference that address.
/// The binding is undefined after the handler.
#if ODD_STRANGER_HAS_EFFECT_OFFSET
#define ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(actorWork, localOffset) ((actorWork)->effectOffset)
#else
#define ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(actorWork, localOffset) (localOffset)
#endif

/// Collapses parts 2..10 after animation has placed the burst's remaining body.
///
/// actorTask is a stable live TMD Task pointer with coordinates through index 10,
/// evaluated once per part in ascending order. Each call samples its part's yaw
/// before replacing pitch, roll and scale with a yaw matrix at Q12 factor 1
/// (1/4096); coefficient quantization can alter the resulting heading.
/// Translation, parent and stored Euler angles stay intact; composition becomes
/// dirty. Each call releases its matrix scratch and clobbers GTE state. Captures
/// no locals. Invoke as a standalone braced statement; undefined below.
#define ODD_STRANGER_COLLAPSE_BURST_PARTS(actorTask)                          \
    {                                                                         \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 2);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 3);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 4);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 5);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 6);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 7);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 8);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 9);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 10); \
    }

/// Walks through three debris spawns, then burns, flattens, fades and hides the corpse.
///
/// Requires live task/work/Enemy and at least model coordinates and rig slots 0..10.
/// Entry makes the target unlockable and starts the walk clip. Debris at frames
/// 3/5/6 inherits placement texture offsets. After frame 16 a slot control jump
/// selects clip 26; its timer stays zero until slot 1 holds a settled pose.
/// The settled sequence releases rewards at 25, starts burning at 30, darkens at
/// 42, fades at 48 and hides at 64. From frame 26 root Y scale decays by 11 Q12
/// units per frame; parts 2..10 remain collapsed. Hidden work remains task-owned.
/// Requires loaded effect resources and initialized scratch storage; GTE state changes.
static void _oddStrangerWalkingDeath(Task* task)
{
    enum {
        ODD_STRANGER_WALK_DEATH_FINAL_CLIP          = 26,
        ODD_STRANGER_WALK_DEATH_TRANSITION_FRAME    = 16,
        ODD_STRANGER_WALK_DEATH_DEBRIS_TASK         = 5,
        ODD_STRANGER_WALK_DEATH_EFFECT_OFFSET       = 100,
        ODD_STRANGER_WALK_DEATH_PARTICLE_ARG        = 0x10300,
        ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE         = 0x200,
        ODD_STRANGER_WALK_DEATH_EVENT_BASE_FRAME    = 25,
        ODD_STRANGER_WALK_DEATH_FIRST_DEBRIS_FRAME  = 3,
        ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_FRAME = 5,
        ODD_STRANGER_WALK_DEATH_THIRD_DEBRIS_FRAME  = 6,
        ODD_STRANGER_WALK_DEATH_EVENT_RELEASE       = 0,
        ODD_STRANGER_WALK_DEATH_EVENT_BURN          = 5,
        ODD_STRANGER_WALK_DEATH_EVENT_DARKEN        = 17,
        ODD_STRANGER_WALK_DEATH_EVENT_FADE          = 23,
        ODD_STRANGER_WALK_DEATH_EVENT_HIDE          = 39,
        ODD_STRANGER_WALK_DEATH_SCALE_START_FRAME   = 26,
        ODD_STRANGER_WALK_DEATH_SCALE_BASE_FRAME    = 20,
        ODD_STRANGER_WALK_DEATH_SCALE_DECAY_Q12     = 11,
        ODD_STRANGER_WALK_DEATH_REWARD_ARG          = 10,
        ODD_STRANGER_WALK_DEATH_BURN_ARG            = 2
    };
#if !ODD_STRANGER_HAS_EFFECT_OFFSET
    SVECTOR effectOffset;
#endif
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              nextFrame;
    s16              currentFrame;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->hitBody.radius                                          = ODD_STRANGER_BODY_RADIUS;
        work->gridBody.flags                                         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                                 = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget                                           = 0;
        work->stateTimer                                              = 0;
        ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vx = ODD_STRANGER_WALK_DEATH_EFFECT_OFFSET;
        ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vz = 0;
        ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vy = 0;
        work->animId                                                  = ODD_STRANGER_ANIM_WALK;
        work->animRequest                                             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                                                = ANIMATION_RATE_ONE;
        effectSpawn(EFFECT_030, task->extra.tmd->coords + 1, ODD_STRANGER_WALK_DEATH_PARTICLE_ARG, &ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset));
        work->stateTimer = 0;
    }
    nextFrame        = work->stateTimer + 1;
    work->stateTimer = nextFrame;
    switch (work->animId) {
        case ODD_STRANGER_ANIM_WALK:
            if ((s16)nextFrame >= ODD_STRANGER_WALK_DEATH_TRANSITION_FRAME && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
                work->animId      = ODD_STRANGER_WALK_DEATH_FINAL_CLIP;
                work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                work->animRate    = ANIMATION_RATE_ONE;
                work->blendActive = 0;
            }
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_WALK_STEP) != 0) {
                _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_WALK_STEP);
            }
            _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
            if ((s16)work->stateTimer == ODD_STRANGER_WALK_DEATH_FIRST_DEBRIS_FRAME) {
                gEffectBurstTaskDescriptors[ODD_STRANGER_WALK_DEATH_DEBRIS_TASK].data.model = &_gOddStrangerBurstPart9Model;
                ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vz               = ODD_STRANGER_WALK_DEATH_EFFECT_OFFSET;
                ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vy               = 0;
                ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vx               = 0;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 9, ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE, &ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset)), enemy);
            }
            if ((s16)work->stateTimer == ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_FRAME) {
                gEffectBurstTaskDescriptors[ODD_STRANGER_WALK_DEATH_DEBRIS_TASK].data.model = ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_MODEL;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 1, ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE, NULL), enemy);
            }
            if ((s16)work->stateTimer == ODD_STRANGER_WALK_DEATH_THIRD_DEBRIS_FRAME) {
                gEffectBurstTaskDescriptors[ODD_STRANGER_WALK_DEATH_DEBRIS_TASK].data.model = &_gOddStrangerBurstPart3Model;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 3, ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE, NULL), enemy);
            }
            break;
        case ODD_STRANGER_WALK_DEATH_FINAL_CLIP:
            if (!(work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                work->stateTimer = 0;
            }
            // These events begin only after the final clip has settled.
            switch ((s16)(work->stateTimer - ODD_STRANGER_WALK_DEATH_EVENT_BASE_FRAME)) {
                case ODD_STRANGER_WALK_DEATH_EVENT_RELEASE:
                    sceneReleaseBattleRefWithRewards(task, ODD_STRANGER_WALK_DEATH_REWARD_ARG);
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_BURN:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords + 2, ODD_STRANGER_WALK_DEATH_BURN_ARG, NULL);
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_FADE:
                    task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_DARKEN:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_HIDE:
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->state            = ODD_STRANGER_STATE_HIDDEN;
                    break;
            }
            currentFrame = work->stateTimer;
            if (currentFrame >= ODD_STRANGER_WALK_DEATH_SCALE_START_FRAME) {
                _actorRenderRescaleYawY(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE, ODD_STRANGER_ROOT_SCALE - (currentFrame - ODD_STRANGER_WALK_DEATH_SCALE_BASE_FRAME) * ODD_STRANGER_WALK_DEATH_SCALE_DECAY_Q12);
            }
            break;
    }
    _oddStrangerDriveAnimation(task);
    ODD_STRANGER_COLLAPSE_BURST_PARTS(task);
}

#undef ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE

#undef ODD_STRANGER_COLLAPSE_BURST_PARTS
