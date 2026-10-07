/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Death-state handler of the first enemy, under the `gSceneCombatState.actorControl` mode byte:
/// mode 2 hides the model and mode 1 does nothing. Otherwise `deathPhase` steps
/// the death through three phases. Phase 0 shrinks the model and counts the
/// kill countdown down; when it runs out the death sound plays, state 0xF0 is
/// released, an optional final effect is spawned, the root transform is saved
/// and the enemy's node and four bodies are unlinked. Phase 1 folds the saved
/// transform back with a decaying Y scale for up to 0x3D frames, and phase 2
/// destroys the enemy once that count is spent. Outside reaction states 5 and 6
/// the first two phases also tick the animation, scale and recompute the
/// second part and re-colour the enemy.
void sucklercephDeathState(Enemy* enemy, Task* task)
{
    TmdObject*       model;
    SucklercephWork* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              soundId;

    obj   = task->extra.tmd;
    work  = task->work;
    coord = obj->coords;
    model = obj;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->deathPhase) {
                case SUCKLERCEPH_DEATH_PHASE_COUNTDOWN:
                    work->blastBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->swellScale      -= 0x12C;
                    task->killCountdown--;
                    if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH && task->killCountdown == 3) {
                        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->state == SUCKLERCEPH_STATE_SLUMP_DEATH) {
                        work->animId = SUCKLERCEPH_ANIM_IDLE;
                        sucklercephTickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->variant != 0) {
                            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000D;
                            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                        } else {
                            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0005;
                            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                        }
                        task->killCountdown = 0;
                        sceneReleaseBattleRefWithRewards(task, 0x2E);
                        if (work->hasBurst != 0) {
                            Gp_SpawnEff(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, NULL);
                        }
                        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_FLATTEN;
                        work->deathFrames   = 0;
                        work->flattenScaleY = ONE;
                        work->savedRootMtx  = coord->coord;
                        enemy->recs         = NULL;
                        worldTargetUnlinkNode(&enemy->node);
                        worldCollisionUnlinkBody(&work->senseBody);
                        worldCollisionUnlinkBody(&work->body);
                        worldCollisionUnlinkBody(&work->attackBody);
                        worldCollisionUnlinkBody(&work->blastBody);
                    }
                    break;
                case SUCKLERCEPH_DEATH_PHASE_FLATTEN:
                    if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH) {
                        work->deathPhase = SUCKLERCEPH_DEATH_PHASE_LINGER;
                    }
                    work->deathFrames++;
                    if (work->deathFrames >= 0x3D) {
                        work->deathPhase = SUCKLERCEPH_DEATH_PHASE_LINGER;
                    }
                    sucklercephFlatten(task);
                    if (work->deathFrames == 0xA) {
                        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    break;
                case SUCKLERCEPH_DEATH_PHASE_LINGER:
                    work->deathFrames++;
                    if (work->deathFrames >= 0x3D) {
                        enemyDestroy(enemy, task);
                    }
                    return;
            }
            if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH) {
                sucklercephTickAnim(task);
                sucklercephScalePart(task, &task->extra.tmd->coords[1]);
                task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
                task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&task->extra.tmd->coords[1]);
                actorUpdateColor(enemy, &task->extra.tmd->coords[1]);
            }
            break;
    }
}
