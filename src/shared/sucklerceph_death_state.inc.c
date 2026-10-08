/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Finishes the death countdown, detaches combat records and expires the corpse.
///
/// Borrows the owning Enemy and live model task. PAUSED does nothing; HIDDEN
/// sets draw suppression and unlockability without progressing death. Running
/// countdown disables the blast sphere and reduces swelling; expiry plays its
/// cue, credits rewards, saves the root and unlinks the target and four bodies.
/// Puffing/slump deaths flatten before lingering; other deaths linger after one
/// flatten tick. The shared counter's expiry threshold is 61; a flatten phase
/// reaching that threshold is destroyed on the next LINGER call. That call
/// destroys task/model/work through `enemyDestroy` and returns.
/// Other running phases refresh part 1 before sampling its cached matrix for
/// lighting, except puffing/slump deaths. No separate shadow draw occurs here.
static void _sucklercephDeathState(Enemy* enemy, Task* task)
{
    TmdObject*       model;
    SucklercephWork* work;
    GfxCoord*        rootCoord;
    s32              soundId;
    enum {
        SUCKLERCEPH_DEATH_SCALE_DECAY_Q12 = 300,
        SUCKLERCEPH_DEATH_HIDE_COUNTDOWN  = 3,
        SUCKLERCEPH_DEATH_LIFETIME_FRAMES = 61,
        SUCKLERCEPH_DEATH_FADE_FRAME      = 10,
        SUCKLERCEPH_DEATH_REWARD_ARG      = 0x2E // Unused compatibility argument; rewards come from enemy parameters.
    };

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    model     = task->extra.tmd;
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
                    // Stop further blast contacts while the visible death reaction settles.
                    work->blastBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->swellScale      -= SUCKLERCEPH_DEATH_SCALE_DECAY_Q12;
                    task->killCountdown--;
                    if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH && task->killCountdown == SUCKLERCEPH_DEATH_HIDE_COUNTDOWN) {
                        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->state == SUCKLERCEPH_STATE_SLUMP_DEATH) {
                        work->animId = SUCKLERCEPH_ANIM_IDLE;
                        _sucklercephTickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->variant != 0) {
                            SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_VARIANT, 13), soundId);
                        } else {
                            SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_DEFAULT, 5), soundId);
                        }
                        task->killCountdown = 0;
                        sceneReleaseBattleRefWithRewards(task, SUCKLERCEPH_DEATH_REWARD_ARG);
                        if (work->hasBurst != 0) {
                            effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, NULL);
                        }
                        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_FLATTEN;
                        work->deathFrames   = 0;
                        work->flattenScaleY = ONE;
                        work->savedRootMtx  = rootCoord->coord;
                        // The corpse retains its model; combat registration ends now.
                        enemy->recs = NULL;
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
                    if (work->deathFrames >= SUCKLERCEPH_DEATH_LIFETIME_FRAMES) {
                        work->deathPhase = SUCKLERCEPH_DEATH_PHASE_LINGER;
                    }
                    _sucklercephFlatten(task);
                    if (work->deathFrames == SUCKLERCEPH_DEATH_FADE_FRAME) {
                        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    break;
                case SUCKLERCEPH_DEATH_PHASE_LINGER:
                    work->deathFrames++;
                    if (work->deathFrames >= SUCKLERCEPH_DEATH_LIFETIME_FRAMES) {
                        enemyDestroy(enemy, task);
                    }
                    return;
            }
            if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH) {
                _sucklercephTickAnim(task);
                _sucklercephScalePart(task, &task->extra.tmd->coords[1]);
                task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
                task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&task->extra.tmd->coords[1]);
                _actorRenderUpdateCoordColor(enemy, &task->extra.tmd->coords[1]);
            }
            break;
    }
}

#undef SUCKLERCEPH_REQUEST_POSITIONED_SOUND
