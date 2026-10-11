/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Limits the alert turn, replaces root yaw and restores normal model scale.
///
/// Requires a live TMD `Task*`, reserved `ActorChaseScratch*` and a nonnegative
/// signed-int limit that fits s16, in 4096ths of a turn. Arguments are evaluated
/// repeatedly and must be stable, side-effect-free and independent of the
/// block-local `headingRoot`. The ordered clamps overwrite `turn`; adding the
/// matrix heading then narrows it to s16 without wrapping to a half turn.
/// Replaces pitch, roll and prior scale with yaw at `ODD_STRANGER_ROOT_SCALE`;
/// translation stays intact and composition becomes dirty. The callees release
/// their matrix scratch and clobber GTE state; chase scratch stays reserved.
/// Invoke as a standalone braced statement.
#define ODD_STRANGER_TURN_ALERT_ROOT(task, alert, turnLimit)                                        \
    {                                                                                               \
        GfxCoord* headingRoot;                                                                      \
                                                                                                    \
        if ((alert)->turn > (turnLimit)) {                                                          \
            (alert)->turn = (turnLimit);                                                            \
        }                                                                                           \
        if ((alert)->turn < -(turnLimit)) {                                                         \
            (alert)->turn = -(turnLimit);                                                           \
        }                                                                                           \
        headingRoot    = (task)->extra.tmd->coords;                                                 \
        (alert)->turn += ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]);           \
        gfxRotMatrixY(&(task)->extra.tmd->coords->coord, (alert)->turn, GRAPHICS_ROTATION_REPLACE); \
        _actorRenderRescaleYaw((task)->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);                 \
    }

/// Raises the combat alert and turns toward the player through the alert clip.
///
/// Handles `ODD_STRANGER_STATE_ALERT` on a live Odd Stranger task with bound
/// rigs and a live player in the same root-parent space. The root turns at most
/// 16 angle units per tick (4096 per turn), while the full bearing feeds the
/// look target. The primary clip boundary selects `CHASE` before playback
/// advances. Borrows one chase scratch block plus the yaw-rescale workspace.
///
/// The Horned Stranger's related `_actor401300StateAlert` also oscillates its
/// fixed-joint pair and gates withdrawal by room and line of sight.
static void _oddStrangerAlert(Task* task)
{
    enum {
        ODD_STRANGER_ALERT_TURN_LIMIT = 16
    };
    OddStrangerWork*   work;
    ActorChaseScratch* alert;
    TmdObject*         model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_ALERT;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        work->hitBody.radius = ODD_STRANGER_BODY_RADIUS;
        sceneEngageBattle(1);
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    alert                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Consume the prior playback boundary before this tick advances the clip.
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    alert->turn         = _actorAngleTurnToPlayer(task, &alert->delta, &gPlayerStatus);
    work->lookYawTarget = alert->turn;
    ODD_STRANGER_TURN_ALERT_ROOT(task, alert, ODD_STRANGER_ALERT_TURN_LIMIT);
    _oddStrangerDriveAnimation(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ODD_STRANGER_TURN_ALERT_ROOT
