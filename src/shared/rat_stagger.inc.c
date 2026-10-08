/* Part of the Rat library; see rat.h. */

/// Pushes the rat away from the player, spins it, then holds or recovers.
///
/// Requires live work/model, the live player matrix in the same parent frame,
/// and an `Enemy` in `Task::spawnArg2.pointer`. The normalized direction includes Y, but only
/// X/Z move: 50 units times that Q12 direction for the first 15 animation
/// frames. Build-up enters its hold directly; ordinary recovery requests an
/// attack after frame 32.
static void _ratStagger(Task* actor)
{
    enum {
        RAT_STAGGER_PUSH_FRAMES      = 15,
        RAT_STAGGER_PUSH_SPEED       = 50,
        RAT_STAGGER_SPIN_START_FRAME = 6,
        RAT_STAGGER_SPIN_TURN_RATE   = 147,
        RAT_STAGGER_SPIN_YAW_STEP    = 1479,
        RAT_STAGGER_RECOVER_FRAME    = 32,
    };

    VECTOR     toPlayer;
    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        durationRandom;
    s32        rootX;

    work      = actor->work;
    model     = actor->extra.tmd;
    rootCoord = model->coords;
    switch (work->step) {
        case RAT_STAGGER_STEP_BEGIN:
            work->animId        = RAT_ANIM_STAGGER;
            work->appliedAnimId = RAT_ANIM_IDLE;
            work->forwardSpeed  = 0;
            work->turnRate      = 0;
            work->knockedDown   = 1;
            work->step          = RAT_STAGGER_STEP_KNOCKBACK;
            durationRandom      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer         = (((u32)durationRandom >> 16) & 0x1F) + RAT_STAGGER_PUSH_FRAMES;
            gRandomLcgState     = durationRandom;
            // Capture the full 3D direction; knockback later applies only X/Z.
            rootX       = rootCoord->coord.t[0];
            toPlayer.vx = gPlayerStatus.coordMtx->t[0] - rootX;
            toPlayer.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
            toPlayer.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            VectorNormalS(&toPlayer, &work->staggerDir);
            break;
        case RAT_STAGGER_STEP_KNOCKBACK:
            if (work->animFrame < RAT_STAGGER_PUSH_FRAMES) {
                rootCoord->coord.t[0] += -(work->staggerDir.vx * RAT_STAGGER_PUSH_SPEED) >> RAT_DIRECTION_FRACTION_BITS;
                rootCoord->coord.t[2] += -(work->staggerDir.vz * RAT_STAGGER_PUSH_SPEED) >> RAT_DIRECTION_FRACTION_BITS;
            }
            if (work->animFrame >= RAT_STAGGER_SPIN_START_FRAME && work->animFrame < RAT_STAGGER_PUSH_FRAMES) {
                work->turnRate  = RAT_STAGGER_SPIN_TURN_RATE;
                work->targetYaw = (work->targetYaw + RAT_STAGGER_SPIN_YAW_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
            } else {
                work->turnRate = 0;
            }
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            if ((((Enemy*)actor->spawnArg2.pointer)->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
                work->animId = RAT_ANIM_BUILDUP_HOLD;
                work->mode   = RAT_MODE_BUILDUP;
                work->step   = RAT_BUILDUP_STEP_HOLD;
                break;
            }
            work->animId = RAT_ANIM_STAGGER_RECOVER;
            work->step   = RAT_STAGGER_STEP_RECOVER;
            break;
        case RAT_STAGGER_STEP_RECOVER:
            if (work->animFrame < RAT_STAGGER_RECOVER_FRAME) {
                break;
            }
            work->mode            = RAT_MODE_IDLE;
            work->step            = RAT_IDLE_STEP_REST;
            work->animId          = RAT_ANIM_IDLE;
            work->timer           = 0;
            work->attackRequested = 1;
            work->knockedDown     = 0;
            break;
    }
}
