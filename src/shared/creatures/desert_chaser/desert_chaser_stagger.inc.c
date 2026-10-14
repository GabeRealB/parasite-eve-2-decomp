/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Plays the armed chaser's stagger, then rises, holds buildup, or starts death.
///
/// Entry restores collision and targeting and starts the build's stagger clip
/// at normal speed. A settled clip sends a living enemy to rise unless buildup
/// is active; an empty HP pool starts the corpse-death sequence.
static void _desertChaserStagger(Task* task)
{
    Enemy*            enemy;
    DesertChaserWork* work;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->hitFlag = 0;
#endif
        model->flags                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                         = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = DESERT_CHASER_CLIP_STAGGER;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = DESERT_CHASER_STATE_STUNNED;
            } else {
                work->state = DESERT_CHASER_STATE_RISE;
            }
        } else {
            work->state = DESERT_CHASER_STATE_DEATH;
        }
    }
}

/// Plays the fatal collapse reaction, then enters the corpse-death state.
///
/// Requires the armed work block and live enemy/model. Entry restores the
/// wall collision sphere, disables targeting, and requests the build's collapse
/// clip at normal speed; slot 1 settling starts the death sequence.
static void _desertChaserCollapse(Task* task)
{
    TmdObject*        model;
    DesertChaserWork* work;
    Enemy*            enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->hitFlag = 0;
#endif
        model->flags                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                         = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = DESERT_CHASER_CLIP_COLLAPSE;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = DESERT_CHASER_STATE_DEATH;
    }
}
