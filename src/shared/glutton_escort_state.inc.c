/* Part of the Glutton library; see glutton.h. */

void gluttonEscortState(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    Enemy*       enemy;
    Enemy*       spawned;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
#else
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
#else
#endif
    s16 i;
    s16 j;
    s16 angle;
    s32 sfx;
    s32 pan;

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    work = (GluttonWork*)arg0->work;
#else
    work = arg0->work;
#endif
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_7B3 = 0xE;
        work->field_7B0 = 1;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        escorts = (GluttonWork*)arg0->work;
#else
        escorts = arg0->work;
#endif
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
#else
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
#endif
            }
        }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        dying = (GluttonWork*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
#else
        tmd   = arg0->extra.tmd;
        dying = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
#endif
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
#else
                escortTmd = dying->field_ECC[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
#endif
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        work->field_EFE = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        work->field_E96 = 0xC80;
#else
#endif
    }
    switch (work->field_6) {
        case 0x64:
        case 0x104:
            if ((s8)work->field_F1A > 0) {
                work->field_7B3 = 0x10;
                work->field_7B0 = 1;
                work->field_EF4 = 1;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
                work->field_F1A--;
#else
                work->field_F1A = work->field_F1A - 1;
#endif
            } else {
                work->field_0   = 0xA;
                work->field_EFE = 0;
            }
            break;
        case 0x74:
            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            SndEvt_EnqueueType6(
                sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#else
            SndEvt_EnqueueType6(sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#endif
            break;
        case 0x1A4:
            work->field_0   = 0xA;
            work->field_EFE = 0;
            work->field_F1A = 0;
            break;
        case 0x9B:
        case 0x113:
            work->field_EFE = 0x80;
            break;
        case 0xAF:
        case 0x145:
            spawned           = Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 3, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->field_EF0   = spawned;
            if (spawned != NULL) {
                gluttonTintEscort(spawned->task->extra.tmd);
                work->field_EFE = 0;
            }
            break;
    }
    coord = arg0->extra.tmd->coords;
    v     = &vec;
    v->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    v->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    v->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    rot   = arg0->extra.tmd->coords;
    angle = ratan2(v->vx, v->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    gluttonTickAnim(arg0);
    if (work->field_7B3 == 0x10 && (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->field_7B3 = 0xE;
        work->field_7B0 = 1;
    }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    if (work->field_6 >= 0x15) {
        work->field_F06 = 3;
    }
#else
#endif
}
