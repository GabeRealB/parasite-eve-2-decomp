/* Part of the web spider library; see web_spider.h. */

/// Dying sequence, the actor task's third state. While the global mode is 1
/// only the colour is updated, and mode 2 sets the model's `field_C` to 0x80.
/// Otherwise the work's `field_39C` steps: step 0 saves the coordinate in
/// `field_370` for the squash, unlinks the context node and the work's four
/// collision objects, passes 0x37 (0x1A when `field_3C0` is clear) to
/// `Gp_ReleaseStateF0Add` and starts animation 0xB; step 1 squashes the model
/// (and, once `field_3BA` has passed 1, frees its buffers and spawns the model
/// effect of `spiderSpawnHusk` in its place), spawns effect 0x600A5 at frame
/// 0xF and moves to step 2 at frame 0x3C; step 2 destroys the enemy 0x3C
/// frames later.
void spiderDyingState(Enemy* arg0, Task* arg1)
{
    VECTOR           vec;
    Actor105500Work* work;
    GfxCoord*        coord;
    GfxCoord*        colorCoord;
    TmdObject*       obj;
    s32              releaseId;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (Gp_StateF0.actorControl) {
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
            switch (work->field_39C) {
                case 0:
                    work->field_3A0 = 0x1000;
                    work->field_370 = coord->coord;
                    arg0->recs      = 0;
                    Gp_UnlinkNode(&arg0->node);
                    Gp_UnlinkObj(&work->field_214);
                    Gp_UnlinkObj(&work->field_294);
                    Gp_UnlinkObj(&work->field_2E4);
                    Gp_UnlinkObj(&work->field_31C);
                    releaseId = 0x37;
                    if (work->field_3C0 == 0) {
                        releaseId = 0x1A;
                    }
                    Gp_ReleaseStateF0Add(arg1, releaseId);
                    Gp_SetStateF0Byte3(2);
                    work->field_39E = 0;
                    work->field_39C = 1;
                    Gp_SetLightMode(arg0, 1);
                    if (work->field_3BA != 0) {
                        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->field_392 = 0xB;
                    spiderTickAnimInline(arg1);
                    colorCoord = arg1->extra.tmd->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
                    return;
                case 1:
                    if (work->field_3BA != 0) {
                        if (work->field_3BA >= 2) {
                            work->field_3BA = 0;
                            Tmd_FreeBuffers(obj);
                            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            spiderSpawnHusk(arg1);
                            spiderShrinkNode2(arg1);
                        } else {
                            work->field_3BA++;
                        }
                    }
                    spiderSquash(arg1);
                    work->field_39E++;
                    if (work->field_39E == 0xA) {
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if (work->field_39E == 0xF) {
                        Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    }
                    if (work->field_39E >= 0x3C) {
                        work->field_39C = 2;
                        work->field_39E = 0;
                        obj->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    spiderTickAnimInline(arg1);
                    colorCoord = arg1->extra.tmd->coords;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
                    return;
                case 2:
                    work->field_39E++;
                    if (work->field_39E >= 0x3C) {
                        Gp_DestroyEnemy(arg0, arg1);
                    }
                    return;
            }
            break;
    }
}
