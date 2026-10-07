/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Knocks the armed chaser down, then waits down, holds buildup, or starts death.
///
/// Entry restores collision and targeting and plays the knock-down clip at
/// the build's reaction rate. Once slot 1 settles on that clip, a living enemy
/// enters the downed wait unless buildup is active; an empty HP pool starts death.
static void _desertChaserKnockDown(Task* task)
{
    enum { DESERT_CHASER_CLIP_KNOCK_DOWN = 0xA };
    Enemy*            enemy;
    DesertChaserWork* work;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        model->flags                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                         = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = DESERT_CHASER_CLIP_KNOCK_DOWN;
        work->animRate                                        = DESERT_CHASER_SLOT_RATE(work);
        work->lookYaw                                         = 0;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
    }
    _desertChaserAnimTick(task);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && (work->animId == DESERT_CHASER_CLIP_KNOCK_DOWN)) {
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = DESERT_CHASER_STATE_STUNNED;
            } else {
                work->state = DESERT_CHASER_STATE_DOWNED;
            }
        } else {
            work->state = DESERT_CHASER_STATE_DEATH;
        }
    }
}
