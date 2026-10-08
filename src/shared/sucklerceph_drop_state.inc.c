/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Advances an armed Sucklerceph drop until it lands and enters crawling.
///
/// Requires live enemy/work/model and coordinate 1's lighting cache. Paused
/// updates sample that cache; hidden updates suppress drawing/targeting.
/// Running updates return until armed, then move, resolve contacts and animate.
/// Lighting samples coordinate 1 before root composition, preserving the cache's
/// existing frame/age. Positive parent-coordinate Y falls; landing clamps Y
/// to zero, stops both speeds and selects crawl animation/update task state 1.
static void _sucklercephDropState(Enemy* enemy, Task* task)
{
    enum {
        SUCKLERCEPH_DROP_COLOR_COORD_INDEX     = 1,
        SUCKLERCEPH_DROP_FORWARD_DECAY         = 2,
        SUCKLERCEPH_DROP_FALL_ACCELERATION     = 10,
        SUCKLERCEPH_DROP_COLLIDED_ACCELERATION = 20,
        SUCKLERCEPH_DROP_LANDING_Y             = 0,
        SUCKLERCEPH_DROP_UPDATE_TASK_STATE     = 1,
        SUCKLERCEPH_DROP_LANDING_SOUND         = 0x402C0008,
        SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT  = 8
    };
    SucklercephWork* work;
    GfxCoord*        rootCoord;
    s32              soundId;

    work = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actorRenderUpdateCoordColor(enemy, &task->extra.tmd->coords[SUCKLERCEPH_DROP_COLOR_COORD_INDEX]);
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->dropArmed == 0) {
                return;
            }
            _sucklercephFallStep(task);
            _sucklercephDropCollide(task);
            _sucklercephTickAnim(task);
            // Keep the cached color sample before composing the moved root.
            _actorRenderUpdateCoordColor(enemy, &task->extra.tmd->coords[SUCKLERCEPH_DROP_COLOR_COORD_INDEX]);
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
            work->forwardSpeed -= SUCKLERCEPH_DROP_FORWARD_DECAY;
            if (work->forwardSpeed < 0) {
                work->forwardSpeed = 0;
            }
            rootCoord = task->extra.tmd->coords;
            if (rootCoord->coord.t[1] >= SUCKLERCEPH_DROP_LANDING_Y) {
                soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT) | SUCKLERCEPH_DROP_LANDING_SOUND;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                work->state                         = SUCKLERCEPH_STATE_AWAKE;
                work->awakeStage                    = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
                work->forwardSpeed                  = 0;
                work->fallSpeed                     = 0;
                work->animId                        = SUCKLERCEPH_ANIM_CRAWL;
                work->appliedAnim                   = 0;
                task->extra.tmd->coords->coord.t[1] = SUCKLERCEPH_DROP_LANDING_Y;
                task->state                         = SUCKLERCEPH_DROP_UPDATE_TASK_STATE;
            } else if (work->dropCollided == 0) {
                work->fallSpeed += SUCKLERCEPH_DROP_FALL_ACCELERATION;
            } else {
                work->fallSpeed += SUCKLERCEPH_DROP_COLLIDED_ACCELERATION;
            }
            break;
    }
}
