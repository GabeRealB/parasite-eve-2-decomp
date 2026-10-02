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
    s16        phase;
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
    st = work->field_37C;
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
    work->field_37E = 6;
    work->field_38C = 0;
    work->field_390 = 0x1000;
    work->field_340 = coord->coord;
    arg0->recs      = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_214);
    Gp_UnlinkObj(&work->field_27C);
    Gp_UnlinkObj(&work->field_2FC);
    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg1, 7);
    work->field_37C = 1;
    work2           = arg1->work;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = gRatAnimBlend[(s16)work2->field_37E];
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend(&work2->anim, i, (s16)work2->field_37E, 0, val);
        }
    } else {
        work2->field_382++;
        for (i = 1; i < 7; i++) {
            animationTickSlot(&work2->anim, i);
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
    phase           = work->field_38C + 1;
    work->field_38C = phase;
    if (phase == 10) {
        obj->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if ((s16)work->field_38C == 15) {
        Gp_SpawnEff(0x600A5, coord, 1, NULL);
    }
    if ((s16)work->field_38C >= 0x3C) {
        work->field_37C = 2;
    }
    work2 = arg1->work;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = gRatAnimBlend[(s16)work2->field_37E];
        for (i = 1; i < 7; i++) {
            animationSeekSlotWithBlend(&work2->anim, i, (s16)work2->field_37E, 0, val);
        }
    } else {
        work2->field_382++;
        for (i = 1; i < 7; i++) {
            animationTickSlot(&work2->anim, i);
        }
    }
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
}
