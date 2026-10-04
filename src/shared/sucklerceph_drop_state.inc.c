/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Per-frame handler of the first enemy while it drops into place. Mode 1 of
/// `gSceneCombatState.actorControl` only re-colours it and mode 2 hides the model. Otherwise, once
/// `dropArmed` has armed the drop, the root steps along its facing and by the
/// fall speed `fallSpeed`, the collision response is applied, the animation
/// ticks and the root is recomputed, with the step length decaying by 2 a
/// frame. Reaching the floor (Y at or above 0) plays the landing sound, pins
/// the root at 0 and moves the enemy to the live stage with animation 2 and
/// task state 1; until then the fall speed grows by 10 a frame, or by 20 once
/// the drop has hit something.
void sucklercephDropState(Enemy* arg0, Task* arg1)
{
    SucklercephWork* work;
    GfxCoord*        coord;
    s32              soundId;

    work = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            actorUpdateColor(arg0, &arg1->extra.tmd->coords[1]);
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->dropArmed == 0) {
                return;
            }
            sucklercephFallStep(arg1);
            sucklercephDropCollide(arg1);
            sucklercephTickAnim(arg1);
            actorUpdateColor(arg0, &arg1->extra.tmd->coords[1]);
            arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg1->extra.tmd->coords);
            work->forwardSpeed -= 2;
            if (work->forwardSpeed < 0) {
                work->forwardSpeed = 0;
            }
            coord = arg1->extra.tmd->coords;
            if (coord->coord.t[1] >= 0) {
                soundId = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0008;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                work->state                         = SUCKLERCEPH_STATE_AWAKE;
                work->awakeStage                    = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
                work->forwardSpeed                  = 0;
                work->fallSpeed                     = 0;
                work->animId                        = SUCKLERCEPH_ANIM_CRAWL;
                work->appliedAnim                   = 0;
                arg1->extra.tmd->coords->coord.t[1] = 0;
                arg1->state                         = 1;
            } else if (work->dropCollided == 0) {
                work->fallSpeed += 10;
            } else {
                work->fallSpeed += 20;
            }
            break;
    }
}
