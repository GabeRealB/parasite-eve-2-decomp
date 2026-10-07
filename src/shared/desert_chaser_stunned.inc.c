/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Holds the chaser in its stun pose until the damage buildup expires.
///
/// Entry resets the build's stun clip and advances synchronously to cue index
/// 12. Later calls halve the signed playback rate and reverse at +/-1, rocking
/// the pose at rates measured in sixteenths of a frame. Buildup expiry clears
/// that reaction flag and selects rise. The stun clip must reach cue 12.
static void _desertChaserStunned(Task* task)
{
    enum { DESERT_CHASER_STUN_HOLD_CUE = 12 };
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;
    s32               halfRate;
    u32               normalReverseRate;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animId                                         = DESERT_CHASER_CLIP_STUNNED;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->animRate                                       = ANIMATION_RATE_ONE;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        // Reach the middle of the stun clip before allowing reverse playback.
        do {
            _desertChaserAnimTick(task);
        } while ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) != DESERT_CHASER_STUN_HOLD_CUE);
        work->animRate = 2 * ANIMATION_RATE_ONE;
        return;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    halfRate                              = (s16)work->animRate / 2;
    work->animRate                        = (u16)halfRate;
    normalReverseRate                     = (u32)ANIMATION_RATE_ONE;
    // Reverse using unsigned arithmetic before narrowing to the stored halfword.
    if (halfRate == 1) {
        work->animRate = -normalReverseRate;
    }
    if ((s16)work->animRate == -1) {
        work->animRate = ANIMATION_RATE_ONE;
    }
    _desertChaserAnimTick(task);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = DESERT_CHASER_STATE_RISE;
    }
}
