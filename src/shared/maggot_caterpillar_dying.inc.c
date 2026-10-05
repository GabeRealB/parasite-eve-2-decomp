/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Dying sequence, the actor task's third state. While the global mode is 1
/// only the colour is updated, and mode 2 sets the model's `field_C` to 0x80.
/// Otherwise the work's `step` steps: step 0 saves the coordinate in
/// `baseMatrix` for the squash, unlinks the context node and the work's four
/// collision objects, passes 0x37 (0x1A when `isCaterpillar` is clear) to
/// `Gp_ReleaseStateF0Add` and starts `MAGGOT_CATERPILLAR_ANIM_HURT`; step 1
/// squashes the model (and, once `burst` has passed 1, frees its buffers and
/// spawns the model effect of `maggotCaterpillarSpawnHusk` in its place),
/// spawns effect 0x600A5 at frame 0xF and moves to step 2 at frame 0x3C; step 2
/// destroys the enemy 0x3C frames later.
void maggotCaterpillarDyingState(Enemy* arg0, Task* arg1)
{
    VECTOR                 vec;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    GfxCoord*              colorCoord;
    TmdObject*             obj;
    s32                    releaseId;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->step) {
                case 0:
                    work->vertical.squashScale = 0x1000;
                    work->baseMatrix           = coord->coord;
                    arg0->recs                 = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    worldCollisionUnlinkBody(&work->gridBody);
                    worldCollisionUnlinkBody(&work->body);
                    worldCollisionUnlinkBody(&work->attackBody);
                    worldCollisionUnlinkBody(&work->flameBody);
                    releaseId = 0x37;
                    if (work->isCaterpillar == 0) {
                        releaseId = 0x1A;
                    }
                    Gp_ReleaseStateF0Add(arg1, releaseId);
                    Gp_SetStateF0Byte3(2);
                    work->stateCounter = 0;
                    work->step         = 1;
                    Gp_SetLightMode(arg0, 1);
                    if (work->burst != 0) {
                        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->animId = MAGGOT_CATERPILLAR_ANIM_HURT;
                    maggotCaterpillarTickAnimInline(arg1);
                    colorCoord = arg1->extra.tmd->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
                    return;
                case 1:
                    if (work->burst != 0) {
                        if (work->burst >= 2) {
                            work->burst = 0;
                            tmdFreePrimitiveBuffer(obj);
                            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            maggotCaterpillarSpawnHusk(arg1);
                            maggotCaterpillarShrinkNode2(arg1);
                        } else {
                            work->burst++;
                        }
                    }
                    maggotCaterpillarSquash(arg1);
                    work->stateCounter++;
                    if (work->stateCounter == 0xA) {
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if (work->stateCounter == 0xF) {
                        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
                    }
                    if (work->stateCounter >= 0x3C) {
                        work->step         = 2;
                        work->stateCounter = 0;
                        obj->flags         = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    maggotCaterpillarTickAnimInline(arg1);
                    colorCoord = arg1->extra.tmd->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
                    return;
                case 2:
                    work->stateCounter++;
                    if (work->stateCounter >= 0x3C) {
                        enemyDestroy(arg0, arg1);
                    }
                    return;
            }
            break;
    }
}
