#include "main/random.h"

/* Part of the Generator library; see generator.h. */

/// Death handler of the main task (its state 2). Death state `deathState` 3 is
/// the wait the hit handler enters when the enemy dies: effects are spawned
/// every fourth frame until message bit 1 moves it to 0. State 0 drops the
/// enemy's lock-on node and both collision objects and starts the sequence;
/// state 1 runs it for 0x78 frames, shrinking the model towards an eighth of
/// its scale while flickering through `gGeneratorHitPulse`, spawning the
/// same two randomly offset effects every fourth frame, setting bit 1 of the
/// model's `field_C` at frame 0x14, spawning effect 0x600A5 at 0x1E and
/// switching the light mode at 0x6E, then ends in state 2. Independently,
/// `battleExitState` 0 calls `sceneReleaseBattleRefWithRewards` once with this sub-state's entry
/// of `gGeneratorReleaseIds` (message bit 2 clears the hold value 2). The
/// pose and colour are ticked every frame, and the enemy is destroyed once the
/// sequence has ended and the release has run.
void generatorDeathState(Enemy* arg0, Task* arg1)
{
    SVECTOR              ofs;
    VECTOR               pos;
    TmdObject*           obj;
    GeneratorWork*       work;
    GfxCoord*            coord;
    GfxCoord*            tmp;
    GeneratorPulseFrame* frame;
    u16                  scale;
    s32                  r;
    s8                   flag;
    s32                  x;
    s32                  z;
    s32                  x2;
    s32                  z2;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    scale = ONE;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
    }
    if ((work->releaseBits & GENERATOR_RELEASE_DEATH) && work->deathState == GENERATOR_DEATH_WAIT) {
        work->deathState = GENERATOR_DEATH_START;
    }
    if ((work->releaseBits & GENERATOR_RELEASE_BATTLE_EXIT) && work->battleExitState == GENERATOR_BATTLE_EXIT_HELD) {
        work->battleExitState = GENERATOR_BATTLE_EXIT_DUE;
    }
    switch (work->deathState) {
        case GENERATOR_DEATH_START:
            work->shrinkScale = ONE;
            work->unscaledMtx = coord->coord;
            arg0->recs        = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->rootBody);
            worldCollisionUnlinkBody(&work->targetBody);
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            if (work->kind == 0) {
                work->stateFrames = 0;
                work->pulseState  = GENERATOR_PULSE_IDLE;
                work->deathState  = GENERATOR_DEATH_SHRINK;
                r                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = r;
            } else {
                work->stateFrames = 0;
                work->pulseState  = GENERATOR_PULSE_IDLE;
                work->deathState  = GENERATOR_DEATH_SHRINK;
                r                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = r;
            }
            flag                                    = 1;
            work->pulseTimer                        = (((u32)r >> 16) & 0xF) + 0xA;
            gSceneCombatState.generatorDeathStarted = flag;
            break;
        case GENERATOR_DEATH_SHRINK:
            if (work->shrinkScale > 0x200) {
                work->shrinkScale -= 0x20;
            }
            switch (work->pulseState) {
                case GENERATOR_PULSE_IDLE:
                    work->pulseTimer--;
                    if (work->pulseTimer <= 0) {
                        work->pulseState = GENERATOR_PULSE_HIT;
                    }
                    scale = work->shrinkScale;
                    break;
                case GENERATOR_PULSE_HIT:
                    frame = &gGeneratorHitPulse[work->pulseTimer];
                    scale = (work->shrinkScale * frame->scale) >> 12;
                    if (frame->last != 0) {
                        work->pulseState = GENERATOR_PULSE_IDLE;
                        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->pulseTimer = ((gRandomLcgState >> 16) & 0xF) + 0xA;
                    } else {
                        work->pulseTimer++;
                    }
                    break;
            }
            _modelPlacementSetScaled(arg1, &work->unscaledMtx, scale, MODEL_PLACEMENT_SCALE_Y_ONLY);
            if (!(work->stateFrames & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx          = x;
                ofs.vy          = -0x9C4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                effectSpawn(EFFECT_FLASH_BURST, coord, 0x400, &ofs);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx          = x2;
                ofs.vy          = -0x960;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                effectSpawn(EFFECT_SMOKE_PUFF, coord, 0x30011600, &ofs);
            }
            work->stateFrames++;
            if (work->stateFrames == 0x14) {
                obj->flags |= TMD_OBJECT_SEMI_TRANS;
            }
            if (work->stateFrames == 0x1E) {
                effectSpawn(EFFECT_CORPSE_BURN, coord, 5, NULL);
            }
            if (work->stateFrames == 0x6E) {
                worldCoordSetActorColorMode(arg0, ENEMY_COLOR_BLACK);
            }
            if (work->stateFrames >= 0x78) {
                work->deathState = GENERATOR_DEATH_DONE;
            }
            tmp    = arg1->extra.tmd->coords;
            pos.vx = tmp->workm.t[0];
            pos.vy = tmp->workm.t[1];
            pos.vz = tmp->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            break;
        case GENERATOR_DEATH_DONE:
            break;
        case GENERATOR_DEATH_WAIT:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->stateFrames++;
            if (!(work->stateFrames & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx          = x;
                ofs.vy          = -0x9C4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                effectSpawn(EFFECT_FLASH_BURST, coord, 0x400, &ofs);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx          = x2;
                ofs.vy          = -0x960;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                effectSpawn(EFFECT_SMOKE_PUFF, coord, 0x30011600, &ofs);
            }
            break;
    }
    if (work->battleExitState == GENERATOR_BATTLE_EXIT_DUE) {
        sceneReleaseBattleRefWithRewards(arg1, gGeneratorReleaseIds[work->kind]);
        work->battleExitState = GENERATOR_BATTLE_EXIT_DONE;
        Gp_ClearAreaFlag4(&gGameSession->location.loc);
    }
    _generatorUpdateAnimationInline(arg1);
    tmp    = arg1->extra.tmd->coords;
    pos.vx = tmp->workm.t[0];
    pos.vy = tmp->workm.t[1];
    pos.vz = tmp->workm.t[2];
    worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
    if (work->deathState == GENERATOR_DEATH_DONE && work->battleExitState == GENERATOR_BATTLE_EXIT_DONE) {
        enemyDestroy(arg0, arg1);
    }
}
