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
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
default_body:
    st = work->step;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    return;
death:
    work->animId       = RAT_ANIM_COLLAPSE;
    work->timer        = 0;
    work->squashScale  = 0x1000;
    work->savedRootMtx = coord->coord;
    arg0->recs         = 0;
    worldTargetUnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->sensorBody);
    Gp_UnlinkObj(&work->hitBody);
    Gp_UnlinkObj(&work->gridBody);
    Gp_UnlinkObj(&work->attackBody);
    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg1, 7);
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
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    snd = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070005;
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    return;
dying:
    ratSquash(arg1);
    work->timer++;
    if (work->timer == 10) {
        obj->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->timer == 15) {
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 1, NULL);
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
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    enemyDestroy(arg0, arg1);
    return;
}
