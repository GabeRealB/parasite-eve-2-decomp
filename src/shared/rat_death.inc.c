/* Part of the Rat library; see rat.h. */

/// Task state 2. While actors are paused it only updates the colour and while
/// hidden it hides the model. Otherwise phase 0 plays animation 6, saves the
/// root transform, unlinks the enemy node and all four collision spheres,
/// releases the state-F0 reference and plays sound 5; phase 1 squashes the
/// model, turns it semi-transparent at frame 10, spawns effect 0x600A5 at frame
/// 15 and moves on at frame 60, after which the enemy is destroyed.
void ratDeath(Enemy* arg0, Task* arg1)
{
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    RatWork*   work2;
    GfxCoord*  c;
    VECTOR     vec;
    s32        state;
    s32        i;
    s16        st;
    s16        val;
    s32        snd;
    s32        pan;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    state = gSceneCombatState.actorControl;
    coord = obj->coords;
    switch (state) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    st = work->step;
    switch (st) {
        case 0:
            work->animId       = RAT_ANIM_COLLAPSE;
            work->timer        = 0;
            work->squashScale  = 0x1000;
            work->savedRootMtx = coord->coord;
            arg0->recs         = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->sensorBody);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->gridBody);
            worldCollisionUnlinkBody(&work->attackBody);
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(arg1, 7);
            work->step = 1;
            work2      = arg1->work;
            if (work2->animId != work2->appliedAnimId) {
                work2->appliedAnimId = work2->animId;
                work2->animFrame     = 0;
                val                  = gRatAnimBlend[work2->animId];
                for (i = 1; i < 7; i++) {
                    animationSeekSlotWithBlend(&work2->rig.anim, i, work2->animId, 0, val);
                }
            } else {
                work2->animFrame++;
                for (i = 1; i < 7; i++) {
                    animationTickSlot(&work2->rig.anim, i);
                }
            }
            c      = arg1->extra.tmd->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070005;
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            _ratSquash(arg1);
            work->timer++;
            if (work->timer == 10) {
                obj->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer == 15) {
                effectSpawn(EFFECT_CORPSE_BURN, coord, 1, NULL);
            }
            if (work->timer >= 0x3C) {
                work->step = 2;
            }
            work2 = arg1->work;
            if (work2->animId != work2->appliedAnimId) {
                work2->appliedAnimId = work2->animId;
                work2->animFrame     = 0;
                val                  = gRatAnimBlend[work2->animId];
                for (i = 1; i < 7; i++) {
                    animationSeekSlotWithBlend(&work2->rig.anim, i, work2->animId, 0, val);
                }
            } else {
                work2->animFrame++;
                for (i = 1; i < 7; i++) {
                    animationTickSlot(&work2->rig.anim, i);
                }
            }
            c      = arg1->extra.tmd->coords;
            vec.vx = c->workm.t[0];
            vec.vy = c->workm.t[1];
            vec.vz = c->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case 2:
            enemyDestroy(arg0, arg1);
            return;
    }
}
