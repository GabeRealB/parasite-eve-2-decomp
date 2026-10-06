/* Part of the Glutton library; see glutton.h. */

void gluttonEscortState(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    Enemy*       enemy;
    Enemy*       spawned;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
    GfxCoord*  coord;
    GfxCoord*  rot;
    TmdObject* tmd;
    TmdObject* escortTmd;
#endif
    SVECTOR  vec;
    SVECTOR* v;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    GfxCoord* coord;
    GfxCoord* rot;
#endif
    s16 i;
    s16 j;
    s16 angle;
    s32 sfx;
    s32 pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->animId           = 0xE;
        work->animStep         = GLUTTON_ANIM_STEP_BLEND;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
#else
        tmd   = arg0->extra.tmd;
        dying = arg0->work;
        if (tmd->buffer == NULL) {
            tmdAllocPrimitiveBuffer(tmd);
        }
#endif
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
#else
                escortTmd = dying->escorts[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    tmdAllocPrimitiveBuffer(escortTmd);
                }
#endif
            }
        }
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 0;
        work->neckPitchTarget  = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        work->wallDistanceTarget = 0xC80;
#endif
    }
    switch (work->stateTicks) {
        case 0x64:
        case 0x104:
            if ((s8)work->pendingHeals > 0) {
                work->animId           = 0x10;
                work->animStep         = GLUTTON_ANIM_STEP_BLEND;
                work->neckPitchEnabled = 1;
                work->pendingHeals--;
            } else {
                work->state           = 0xA;
                work->neckPitchTarget = 0;
            }
            break;
        case 0x74:
            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(
                sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            break;
        case 0x1A4:
            work->state           = 0xA;
            work->neckPitchTarget = 0;
            work->pendingHeals    = 0;
            break;
        case 0x9B:
        case 0x113:
            work->neckPitchTarget = 0x80;
            break;
        case 0xAF:
        case 0x145:
            spawned           = Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 3, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->lastSpawned = spawned;
            if (spawned != NULL) {
                gluttonTintEscort(spawned->task->extra.tmd);
                work->neckPitchTarget = 0;
            }
            break;
    }
    coord               = arg0->extra.tmd->coords;
    v                   = &vec;
    v->vx               = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    v->vy               = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    v->vz               = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    rot                 = arg0->extra.tmd->coords;
    angle               = ratan2(v->vx, v->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
    angle               = actorWrapAngle(angle);
    work->neckYawTarget = angle;
    gluttonTickAnim(arg0);
    if (work->animId == 0x10 && (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId   = 0xE;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    if (work->stateTicks >= 0x15) {
        work->viewSelector = 3;
    }
#endif
}
