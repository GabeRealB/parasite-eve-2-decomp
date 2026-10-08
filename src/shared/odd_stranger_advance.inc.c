/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Limits the slide turn and replaces the root yaw without rescaling it.
///
/// Requires a live `Task*` and reserved `ActorTurnScratch*` as side-effect-free
/// pointer expressions, and a positive limit in 4096ths of a turn. Arguments
/// occur repeatedly; the scratch angle becomes the absolute root heading.
/// Translation stays intact. Expands to a block; invoke inside a braced block.
#define ODD_STRANGER_TURN_SLIDE_ROOT(task, turn, turnLimit)                                         \
    {                                                                                               \
        GfxCoord* headingRoot;                                                                      \
                                                                                                    \
        if ((turn)->angle > (turnLimit)) {                                                          \
            (turn)->angle = (turnLimit);                                                            \
        }                                                                                           \
        if ((turn)->angle < -(turnLimit)) {                                                         \
            (turn)->angle = -(turnLimit);                                                           \
        }                                                                                           \
        headingRoot    = (task)->extra.tmd->coords;                                                 \
        (turn)->angle += ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]);           \
        gfxRotMatrixY(&(task)->extra.tmd->coords->coord, (turn)->angle, GRAPHICS_ROTATION_REPLACE); \
    }

/// Coasts toward the player with the circle dash's remaining forward step.
///
/// Handles `ODD_STRANGER_STATE_SLIDE` on a live Odd Stranger task with bound
/// animation rigs and a live player in the same root-parent coordinate space.
/// Turns at most 64 angle units per tick (4096 per turn), preserves the full
/// turn as the look target, and reduces the inherited step by 10 coordinate
/// units per tick. The clip boundary or a spent step selects `TURN_AROUND`.
/// Borrows one turn scratch block plus the nested movement/contact workspace.
static void _oddStrangerSlide(Task* task)
{
    enum {
        ODD_STRANGER_SLIDE_ANIM         = 18,
        ODD_STRANGER_SLIDE_RATE         = 30, // Sixteenths of a frame per tick
        ODD_STRANGER_SLIDE_TURN_LIMIT   = 64, // 4096 units per turn
        ODD_STRANGER_SLIDE_DECELERATION = 10
    };
    OddStrangerWork*  work;
    Enemy*            enemy;
    TmdObject*        model;
    ActorTurnScratch* turn;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy             = task->spawnArg2.pointer;
        model             = task->extra.tmd;
        work->animId      = ODD_STRANGER_SLIDE_ANIM;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        model->flags      = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ODD_STRANGER_SLIDE_RATE;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    // The look keeps the full player bearing; only the root turn is limited.
    turn->angle         = _actorAngleTurnToPlayer(task, &turn->delta, &gPlayerStatus);
    work->lookYawTarget = turn->angle;
    ODD_STRANGER_TURN_SLIDE_ROOT(task, turn, ODD_STRANGER_SLIDE_TURN_LIMIT);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if ((_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, work->slideStep) << 0x10) != 0) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, (u16)work->slideStep);
    }
    if (work->slideStep > 0) {
        work->slideStep = (u16)work->slideStep - ODD_STRANGER_SLIDE_DECELERATION;
        if (work->slideStep < 0) {
            work->slideStep = 0;
        }
    }
    _oddStrangerDriveAnimation(task);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) || work->slideStep == 0) {
        work->state = ODD_STRANGER_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

#undef ODD_STRANGER_TURN_SLIDE_ROOT
