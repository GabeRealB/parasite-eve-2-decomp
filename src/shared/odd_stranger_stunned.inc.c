/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Oscillates a lying pose while `ODD_STRANGER_STATE_STATUS_HOLD` lasts.
///
/// Entry restores the model and targeting, resets a back/front lying clip,
/// and advances to cue 6/9 before allowing reverse ticks. Later calls halve
/// the signed rate and switch at +/-1 to the opposite normal-rate direction.
/// Expired buildup selects `ODD_STRANGER_STATE_DOWN`; variant 1 also exits
/// when HP is nonpositive. Requires loaded lying clips and live work and enemy.
static void _oddStrangerStatusHold(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       tmd;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        tmd                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->animRequest     = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate        = ANIMATION_RATE_ONE;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if ODD_STRANGER_VARIANT == 1
        if (work->animId == ODD_STRANGER_ANIM_DOWN_BACK || work->animId == ODD_STRANGER_ANIM_STATUS_BACK) {
#else
        if (work->animId == ODD_STRANGER_ANIM_DOWN_BACK) {
#endif
            work->animId = ODD_STRANGER_ANIM_STATUS_BACK;
#if ODD_STRANGER_VARIANT == 1
        } else if (work->animId == ODD_STRANGER_ANIM_DOWN_FRONT || work->animId == ODD_STRANGER_ANIM_REFALL_FRONT || work->animId == ODD_STRANGER_ANIM_STATUS_FRONT) {
#else
        } else if (work->animId == ODD_STRANGER_ANIM_DOWN_FRONT || work->animId == ODD_STRANGER_ANIM_REFALL_FRONT) {
#endif
            work->animId = ODD_STRANGER_ANIM_STATUS_FRONT;
        }
        if ((u16)(work->animId - ODD_STRANGER_ANIM_STATUS_BACK) >= 2) {
            work->animId = ODD_STRANGER_ANIM_STATUS_BACK;
        }
        // Start from bank records and advance beyond the initial pose before reversing.
        do {
            _oddStrangerDriveAnimation(task);
            if (work->animId == ODD_STRANGER_ANIM_STATUS_BACK && (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 6) {
                break;
            }
            if (work->animId == ODD_STRANGER_ANIM_STATUS_FRONT && (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 9) {
                break;
            }
#if ODD_STRANGER_VARIANT == 1
        } while (!(work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)));
#else
        } while (1);
#endif
        work->animRate = 2 * ANIMATION_RATE_ONE;
        return;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->animRate                        = work->animRate / 2;
    if (work->animRate == 1) {
        work->animRate = -ANIMATION_RATE_ONE;
    }
    if (work->animRate == -1) {
        work->animRate = ANIMATION_RATE_ONE;
    }
    _oddStrangerDriveAnimation(task);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        work->state           = ODD_STRANGER_STATE_DOWN;
    }
#if ODD_STRANGER_VARIANT == 1
    if (enemy->hp <= 0) {
        work->state = ODD_STRANGER_STATE_DOWN;
    }
#endif
}
